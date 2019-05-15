#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#define SET_WEIGHT_NUM 398
#define GET_WEIGHT_NUM 399
#define SCHED_WRR 7

int main(void) {
  int syscallResult;
  cpu_set_t mask;
  struct sched_param dummy = {
    .sched_priority = 0
  };

  printf("Move task's CPU to 3...\n");
  CPU_ZERO(&mask);
  CPU_SET(3, &mask);
  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
  sleep(2);
  printf("Success! CPU of task = %d\n\n", sched_getcpu());

  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  CPU_SET(1, &mask);
  CPU_SET(2, &mask);
  CPU_SET(3, &mask);
  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);

  printf("Now change sched policy to SCHED_WRR\n");
  syscallResult = sched_setscheduler(0, SCHED_WRR, &dummy);
  sleep(2);
  printf("Success! CPU of task = %d (moved!)\n\n", sched_getcpu());

  CPU_ZERO(&mask);
  CPU_SET(3, &mask);
  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
  printf("Try move task's CPU to 3...\n");
  sleep(2);
  printf("Syscall Result = %d (failed!) current CPU of task = %d\n\n", syscallResult, sched_getcpu());

  printf("Now change sched policy to SCHED_OTHER\n");
  syscallResult = sched_setscheduler(0, SCHED_OTHER, &dummy);
  sleep(2);
  printf("Success! CPU of task = %d\n\n", sched_getcpu());

  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
  printf("Try move task's CPU to 3...\n");
  sleep(2);
  printf("Success! CPU of task = %d (moved!)\n\n", sched_getcpu());
}
