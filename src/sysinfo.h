#pragma once

#include <stddef.h>
#include <stdint.h>

// Memory usage of the current process and of the system as a whole.
// All sizes are in bytes.
struct MemoryInfo {
    uint64_t process_rss;      // resident set size (physical memory in use)
    uint64_t process_peak_rss; // peak resident set size
    uint64_t process_vsize;    // virtual address space size
    uint64_t system_total;     // total physical memory of the machine
};

// Fills out with the current memory usage. Returns 0 on success, -1 if the
// information is not available on this platform.
int SysGetMemoryInfo(struct MemoryInfo *out);

// Writes a human readable size (e.g. "12.3 MiB") into buf.
void SysFormatBytes(uint64_t bytes, char *buf, size_t buf_size);