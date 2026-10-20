#include "sysinfo.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
// Sums up the committed pages of the address space of the current process.
// Used as a fallback when GetProcessMemoryInfo() doesn't report a working set.
static uint64_t GetWorkingSetSize(void) {
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uint64_t total = 0;
    uintptr_t addr = 0;
    while (addr < (uintptr_t)si.lpMaximumApplicationAddress) {
        if (VirtualQuery((void*)addr, &mbi, sizeof(mbi)) == 0) break;
        if (mbi.State == MEM_COMMIT)
            total += mbi.RegionSize;
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;   // no progress, stop
        addr = next;
    }
    return total;
}
#endif

int SysGetMemoryInfo(struct MemoryInfo *out) {
    memset(out, 0, sizeof(*out));

#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);   // required by the API
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        out->process_rss = pmc.WorkingSetSize;
        out->process_peak_rss = pmc.PeakWorkingSetSize;
        out->process_vsize = pmc.PagefileUsage;
    } else {
        return -1;
    }
    // Some environments don't fill in the working set; compute it by walking
    // the address space of the process instead.
    if (out->process_rss == 0)
        out->process_rss = GetWorkingSetSize();
    MEMORYSTATUSEX mem;
    memset(&mem, 0, sizeof(mem));
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem))
        out->system_total = mem.ullTotalPhys;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
        return -1;
    out->process_rss = info.resident_size;
    out->process_vsize = info.virtual_size;
    out->process_peak_rss = info.resident_size_max;

    uint64_t total = 0;
    size_t len = sizeof(total);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &total, &len, NULL, 0) == 0)
        out->system_total = total;
#else
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    // VmRSS is the current resident set size, VmHWM the peak one.
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long kb = 0;
        if (sscanf(line, "VmRSS: %llu kB", &kb) == 1)
            out->process_rss = kb * 1024;
        else if (sscanf(line, "VmHWM: %llu kB", &kb) == 1)
            out->process_peak_rss = kb * 1024;
    }
    fclose(f);

    // /proc/self/statm provides the virtual address space size.
    FILE *sf = fopen("/proc/self/statm", "r");
    if (sf) {
        unsigned long long total_pages = 0, rss_pages = 0;
        if (fscanf(sf, "%llu %llu", &total_pages, &rss_pages) == 2) {
            long page_size = sysconf(_SC_PAGESIZE);
            if (page_size <= 0) page_size = 4096;
            out->process_vsize = (uint64_t)total_pages * (uint64_t)page_size;
            if (out->process_rss == 0)
                out->process_rss = (uint64_t)rss_pages * (uint64_t)page_size;
        }
        fclose(sf);
    }

    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size_sys = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size_sys > 0)
        out->system_total = (uint64_t)pages * (uint64_t)page_size_sys;
#endif
    return 0;
}

void SysFormatBytes(uint64_t bytes, char *buf, size_t buf_size) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB"};
    double value = (double)bytes;
    int unit = 0;
    while (value >= 1024.0 && unit < (int)(sizeof(units)/sizeof(units[0])) - 1) {
        value /= 1024.0;
        unit++;
    }
    if (unit == 0)
        snprintf(buf, buf_size, "%.0f %s", value, units[unit]);
    else
        snprintf(buf, buf_size, "%.1f %s", value, units[unit]);
}