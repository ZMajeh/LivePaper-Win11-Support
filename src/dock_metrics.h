#pragma once

#include <stdint.h>

// The values of the system that are shown at the right of the dock bar.
//
// The values are sampled once per second, not once per frame, so that the
// program stays quiet.

// The load of the processor, in percent (0-100), or -1 if it is not known.
extern int g_metrics_cpu_percent;

// The used memory of the system and the total memory, in bytes.
extern uint64_t g_metrics_ram_used;
extern uint64_t g_metrics_ram_total;

// The speed of the network, in bytes per second.
extern uint64_t g_metrics_net_up;
extern uint64_t g_metrics_net_down;

// The load of the disks, in percent (0-100), or -1 if it is not known.
extern int g_metrics_disk_percent;

// The time of the day, as "HH:MM".
extern char g_metrics_clock[8];

// Samples the values. It has to be called once per second. The first call
// only fills the counters that are needed to compute the differences.
void MetricsSample(void);

// The number of the samples that were taken.
extern int g_metrics_samples;
