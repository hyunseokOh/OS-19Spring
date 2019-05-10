#ifndef _LINUX_SCHED_WRR_H
#define _LINUX_SCHED_WRR_H

#include <linux/sched.h>
#include <linux/threads.h>

#define WRR_DEFAULT_WEIGHT 10
/* 10 ms */
#define WRR_BASE_TIMESLICE (10 * HZ / 1000)
#define WRR_TIMESLICE(weight) (weight * WRR_BASE_TIMESLICE)

#define FORBIDDEN_WRR_QUEUE (NR_CPUS - 1)

#define WRR_REQUEUE_LOAD_BALANCE 0
#define WRR_REQUEUE_YIELD 1

#define WRR_GET_MIN 0
#define WRR_GET_MAX 1


extern int get_target_cpu(int flag, int rcu_lock, struct task_struct *p);

#endif
