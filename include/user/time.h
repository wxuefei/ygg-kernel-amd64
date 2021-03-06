#pragma once
#include <stdint.h>

struct timeval {
    uint64_t tv_sec;
    uint32_t tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

struct timespec {
    uint64_t tv_sec;
    long tv_nsec;
};

typedef int64_t time_t;

// TODO: move to kernel header
#if defined(__KERNEL__)
struct tm;

// Nanoseconds since boot
extern uint64_t system_time;
// Seconds since epoch
extern time_t system_boot_time;

time_t mktime(struct tm *tm);
time_t time(void);
#endif
