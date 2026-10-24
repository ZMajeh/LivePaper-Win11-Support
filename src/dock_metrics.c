#include "dock_metrics.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>
#include <pdh.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

int g_metrics_cpu_percent = -1;
uint64_t g_metrics_ram_used = 0;
uint64_t g_metrics_ram_total = 0;
uint64_t g_metrics_net_up = 0;
uint64_t g_metrics_net_down = 0;
int g_metrics_disk_percent = -1;
char g_metrics_clock[8] = "00:00";
int g_metrics_samples = 0;

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
#ifdef _WIN32

// The counters of the last sample.
static ULONGLONG s_last_idle = 0, s_last_kernel = 0, s_last_user = 0;
static bool s_have_last = false;

// The number of the milliseconds since the start of the system.
static ULONGLONG SystemMillis(void) {
    return GetTickCount64();
}

// Samples the load of the processor with GetSystemTimes(). The idle time and
// the total time are compared with the values of the previous sample.
static void SampleCpuWindows(void) {
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        g_metrics_cpu_percent = -1;
        return;
    }
    ULONGLONG idle = ((ULONGLONG)idle_time.dwHighDateTime << 32) | idle_time.dwLowDateTime;
    ULONGLONG kernel = ((ULONGLONG)kernel_time.dwHighDateTime << 32) | kernel_time.dwLowDateTime;
    ULONGLONG user = ((ULONGLONG)user_time.dwHighDateTime << 32) | user_time.dwLowDateTime;

    if (!s_have_last) {
        s_last_idle = idle;
        s_last_kernel = kernel;
        s_last_user = user;
        g_metrics_cpu_percent = 0;
        return;
    }

    ULONGLONG idle_diff = idle - s_last_idle;
    ULONGLONG total_diff = (kernel - s_last_kernel) + (user - s_last_user);
    s_last_idle = idle;
    s_last_kernel = kernel;
    s_last_user = user;

    if (total_diff == 0) {
        g_metrics_cpu_percent = 0;
        return;
    }
    // The kernel time includes the idle time.
    int percent = (int)(((total_diff - idle_diff) * 100) / total_diff);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_metrics_cpu_percent = percent;
}

// The used memory of the system.
static void SampleRamWindows(void) {
    MEMORYSTATUSEX status;
    memset(&status, 0, sizeof(status));
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        g_metrics_ram_used = 0;
        g_metrics_ram_total = 0;
        return;
    }
    g_metrics_ram_total = status.ullTotalPhys;
    g_metrics_ram_used = status.ullTotalPhys - status.ullAvailPhys;
}

// The counters of the network of the last sample. They are 32 bits wide, so
// the difference is computed with the 32 bits arithmetic: it wraps around
// exactly like the counters of Windows do.
struct NetCounter {
    DWORD in_octets;
    DWORD out_octets;
    bool valid;
};

static struct NetCounter *s_net_counters = NULL;
static DWORD s_net_counter_count = 0;

// The number of the bytes that went in and out of the network.
static bool SampleNetWindows(ULONGLONG *in, ULONGLONG *out) {
    // The size of the table is asked first, so that the table can be
    // allocated with the right size.
    ULONG size = 0;
    if (GetIfTable(NULL, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) return false;
    if (size == 0) return false;

    void *buffer = malloc(size);
    if (!buffer) return false;
    if (GetIfTable((PMIB_IFTABLE)buffer, &size, FALSE) != NO_ERROR) {
        free(buffer);
        return false;
    }

    // The rows of the table follow the number of the entries. The member that
    // holds them has a different name in the headers of MSVC and of MinGW, so
    // the layout of the table is spelled out here.
    struct IfTable {
        DWORD count;
        MIB_IFROW rows[1];
    };
    struct IfTable *table = buffer;

    // The table of the last sample is grown, if it is too small.
    if (table->count > s_net_counter_count) {
        struct NetCounter *grown = realloc(s_net_counters,
                                           sizeof(struct NetCounter) * table->count);
        if (!grown) {
            free(buffer);
            return false;
        }
        memset(grown + s_net_counter_count, 0,
               sizeof(struct NetCounter) * (table->count - s_net_counter_count));
        s_net_counters = grown;
        s_net_counter_count = table->count;
    }

    ULONGLONG sum_in = 0, sum_out = 0;
    // The first row is the loopback interface of the system.
    for (DWORD i = 1; i < table->count; i++) {
        MIB_IFROW *row = &table->rows[i];
        // The loopback interface is not a real network.
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        struct NetCounter *counter = &s_net_counters[i];
        if (counter->valid) {
            // The 32 bits arithmetic wraps around, like the counters of
            // Windows do.
            sum_in += (ULONGLONG)(DWORD)(row->dwInOctets - counter->in_octets);
            sum_out += (ULONGLONG)(DWORD)(row->dwOutOctets - counter->out_octets);
        }
        counter->in_octets = row->dwInOctets;
        counter->out_octets = row->dwOutOctets;
        counter->valid = true;
    }
    free(buffer);
    *in = sum_in;
    *out = sum_out;
    return true;
}

// The load of the disks: the time that the disks were busy, compared with
// the time that has passed. The value comes from the performance counters of
// Windows. The English name of the counter is used, so that the program
// works on a system with another language.
static void SampleDiskWindows(void) {
    g_metrics_disk_percent = -1;

    static bool initialized = false;
    static PDH_HQUERY query = NULL;
    static PDH_HCOUNTER counter = NULL;

    if (!initialized) {
        initialized = true;
        if (PdhOpenQueryA(NULL, 0, &query) != ERROR_SUCCESS) {
            query = NULL;
            return;
        }
        if (PdhAddEnglishCounterA(query, "\\\\PhysicalDisk(_Total)\\% Disk Time",
                                  0, &counter) != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            query = NULL;
            return;
        }
    }
    if (!query || !counter) return;

    // The counter is a rate: it needs two samples to have a value.
    if (PdhCollectQueryData(query) != ERROR_SUCCESS) return;
    PDH_FMT_COUNTERVALUE value;
    memset(&value, 0, sizeof(value));
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100,
                                    NULL, &value) != ERROR_SUCCESS) {
        return;
    }
    int percent = (int)(value.doubleValue + 0.5);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_metrics_disk_percent = percent;
}

// The time of the day.
static void SampleClockWindows(void) {
    SYSTEMTIME time;
    GetLocalTime(&time);
    snprintf(g_metrics_clock, sizeof(g_metrics_clock), "%02d:%02d",
             time.wHour, time.wMinute);
}

#endif  // _WIN32

// ---------------------------------------------------------------------------
// Linux
// ---------------------------------------------------------------------------
#ifndef _WIN32
#ifndef __APPLE__

// The counters of the last sample.
static unsigned long long s_last_idle = 0, s_last_total = 0;
static unsigned long long s_last_net_in = 0, s_last_net_out = 0;
static unsigned long long s_last_disk_busy = 0;
static bool s_have_last = false;

// The number of the milliseconds since the start of the system.
static unsigned long long SystemMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// The load of the processor, from the first line of /proc/stat.
static void SampleCpuLinux(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        g_metrics_cpu_percent = -1;
        return;
    }
    char line[512];
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    bool found = false;
    if (fgets(line, sizeof(line), f)) {
        // The line starts with the word "cpu", then the counters follow.
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait) == 5) {
            found = true;
        }
    }
    fclose(f);
    if (!found) {
        g_metrics_cpu_percent = -1;
        return;
    }
    unsigned long long total = user + nice + system + idle + iowait;
    unsigned long long idle_all = idle + iowait;
    if (!s_have_last) {
        s_last_idle = idle_all;
        s_last_total = total;
        g_metrics_cpu_percent = 0;
        return;
    }
    unsigned long long total_diff = total - s_last_total;
    unsigned long long idle_diff = idle_all - s_last_idle;
    s_last_idle = idle_all;
    s_last_total = total;
    if (total_diff == 0) {
        g_metrics_cpu_percent = 0;
        return;
    }
    int percent = (int)(((total_diff - idle_diff) * 100) / total_diff);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_metrics_cpu_percent = percent;
}

// The used memory of the system, from /proc/meminfo.
static void SampleRamLinux(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        g_metrics_ram_used = 0;
        g_metrics_ram_total = 0;
        return;
    }
    unsigned long long total = 0, available = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long value = 0;
        if (sscanf(line, "MemTotal: %llu kB", &value) == 1) {
            total = value * 1024;
        } else if (sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
            available = value * 1024;
        }
    }
    fclose(f);
    g_metrics_ram_total = total;
    g_metrics_ram_used = total > available ? total - available : 0;
}

// The number of the bytes that went in and out of the network, from
// /proc/net/dev.
static bool SampleNetLinux(unsigned long long *in, unsigned long long *out) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return false;
    unsigned long long sum_in = 0, sum_out = 0;
    char line[512];
    // The first two lines are the header.
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        unsigned long long rx = 0, tx = 0;
        // The line looks like this:
        //   eth0: 123456 78 0 0 0 0 0 654321 90 0 0 0 0 0 0
        // The first number is the received and the ninth the sent bytes: the
        // eight numbers of the received bytes are skipped in between.
        if (sscanf(line, " %63[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu",
                   name, &rx, &tx) != 3) {
            continue;
        }
        // The loopback interface is not a real network.
        if (strcmp(name, "lo") == 0) continue;
        sum_in += rx;
        sum_out += tx;
    }
    fclose(f);
    *in = sum_in;
    *out = sum_out;
    return true;
}

// The number of the milliseconds that the disks were busy, from
// /proc/diskstats. The eleventh number of a line is the time that the disk
// spent doing the input and the output.
static bool SampleDiskLinux(unsigned long long *busy) {
    FILE *f = fopen("/proc/diskstats", "r");
    if (!f) return false;
    unsigned long long sum = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        unsigned long long major = 0, minor = 0;
        unsigned long long values[8] = {0};
        // The name is the third number, the counters follow it.
        int matched = sscanf(line, "%llu %llu %63s %llu %llu %llu %llu %llu %llu %llu %llu",
                             &major, &minor, name,
                             &values[0], &values[1], &values[2], &values[3],
                             &values[4], &values[5], &values[6], &values[7]);
        if (matched < 11) continue;
        // The partitions of a disk are counted twice, so only the whole
        // disks are used.
        if (strncmp(name, "loop", 4) == 0) continue;
        if (strncmp(name, "ram", 3) == 0) continue;
        if (strchr(name, 'p') || strchr(name, 'n')) continue;
        sum += values[7];
    }
    fclose(f);
    *busy = sum;
    return true;
}

// The time of the day.
static void SampleClockLinux(void) {
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    snprintf(g_metrics_clock, sizeof(g_metrics_clock), "%02d:%02d",
             local.tm_hour, local.tm_min);
}

#endif  // !__APPLE__
#endif  // !_WIN32

// ---------------------------------------------------------------------------
// macOS
// ---------------------------------------------------------------------------
#ifdef __APPLE__

// The load of the processor: the values of the host statistics are used.
static void SampleCpuMac(void) {
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        (host_info_t)&info, &count) != KERN_SUCCESS) {
        g_metrics_cpu_percent = -1;
        return;
    }
    if (info.cpu_ticks[CPU_STATE_USER] == 0) {
        g_metrics_cpu_percent = 0;
        return;
    }
    unsigned int total = info.cpu_ticks[CPU_STATE_USER] +
                         info.cpu_ticks[CPU_STATE_SYSTEM] +
                         info.cpu_ticks[CPU_STATE_IDLE];
    unsigned int used = info.cpu_ticks[CPU_STATE_USER] +
                        info.cpu_ticks[CPU_STATE_SYSTEM];
    int percent = total ? (int)((used * 100) / total) : 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_metrics_cpu_percent = percent;
}

// The used memory of the system, with the virtual memory statistics.
static void SampleRamMac(void) {
    vm_statistics64_data_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics(mach_host_self(), HOST_VM_INFO64,
                        (host_info_t)&stats, &count) != KERN_SUCCESS) {
        g_metrics_ram_used = 0;
        g_metrics_ram_total = 0;
        return;
    }
    vm_size_t page_size = 4096;
    host_page_size(mach_host_self(), &page_size);
    uint64_t total = (uint64_t)stats.phys_mem_size;
    uint64_t free_pages = (uint64_t)stats.free_count + (uint64_t)stats.inactive_count;
    uint64_t used = total > free_pages * page_size ? total - free_pages * page_size : 0;
    g_metrics_ram_total = total;
    g_metrics_ram_used = used;
}

// The time of the day.
static void SampleClockMac(void) {
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    snprintf(g_metrics_clock, sizeof(g_metrics_clock), "%02d:%02d",
             local.tm_hour, local.tm_min);
}

#endif  // __APPLE__

void MetricsSample(void) {
    g_metrics_samples++;

#ifdef _WIN32
    SampleCpuWindows();
    SampleRamWindows();
    SampleClockWindows();
    SampleDiskWindows();

    // The speed of the network is the difference between the two samples.
    ULONGLONG net_in = 0, net_out = 0;
    if (SampleNetWindows(&net_in, &net_out)) {
        g_metrics_net_up = net_in;
        g_metrics_net_down = net_out;
    }
    s_have_last = true;
#else
#ifdef __APPLE__
    SampleCpuMac();
    SampleRamMac();
    SampleClockMac();
    // The network and the disks are not sampled on macOS yet.
    g_metrics_net_up = 0;
    g_metrics_net_down = 0;
    g_metrics_disk_percent = -1;
#else
    SampleCpuLinux();
    SampleRamLinux();
    SampleClockLinux();

    unsigned long long net_in = 0, net_out = 0;
    if (SampleNetLinux(&net_in, &net_out)) {
        if (s_have_last) {
            g_metrics_net_up = net_in - s_last_net_in;
            g_metrics_net_down = net_out - s_last_net_out;
        }
        s_last_net_in = net_in;
        s_last_net_out = net_out;
    }

    // The load of the disks is the share of the time that they were busy.
    unsigned long long busy = 0;
    if (SampleDiskLinux(&busy)) {
        unsigned long long now = SystemMillis();
        static unsigned long long last_time = 0;
        if (s_have_last && now > last_time && busy >= s_last_disk_busy) {
            unsigned long long time_diff = now - last_time;
            unsigned long long busy_diff = busy - s_last_disk_busy;
            int percent = (int)((busy_diff * 100) / time_diff);
            if (percent < 0) percent = 0;
            if (percent > 100) percent = 100;
            g_metrics_disk_percent = percent;
        }
        s_last_disk_busy = busy;
        last_time = now;
    }
    s_have_last = true;
#endif
#endif
}
