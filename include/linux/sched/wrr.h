#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>
#include <linux/threads.h>

#define WRR_DEFAULT_WEIGHT 10
#define WRR_DEFAULT_TIMESLICE 10 /* FIXME (its ms unit) */
#define FORBIDDEN_WRR_QUEUE (NR_CPUS - 1)

#endif
