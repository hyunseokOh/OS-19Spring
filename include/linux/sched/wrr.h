#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>
#include <linux/threads.h>

#define WRR_DEFAULT_WEIGHT 10
/* 10 ms */
#define WRR_BASE_TIMESLICE (10 * HZ / 1000)
#define WRR_TIMESLICE(weight) (weight * WRR_BASE_TIMESLICE)
#define FORBIDDEN_WRR_QUEUE (NR_CPUS - 1)

#endif
