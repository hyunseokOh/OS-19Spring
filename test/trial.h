/*
 * All helper functions for test
 */
#ifndef _TRIAL_H
#define _TRIAL_H

#define TARGET 499999991
#define ITER 1000
#define SCHED_WRR 7
#define CPUS 3
#define SET_WEIGHT_NUM 398
#define GET_WEIGHT_NUM 399
#define GET_EACH_WEIGHT 400

#include <sched.h>
#include <stdio.h>
#include <time.h>
#include <syscall.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

void factorization_no_print(int target) {
  int divider;
  int quotient;
  int remainder;

  /* edge cases */
  if (target == 0 || target == 1 || target == -1) {
  }

  if (target < 0) {
    target = -target;
  } else {
  }

  divider = 2;
  while (target > 1) {
    remainder = target % divider;
    if (remainder == 0) {
      /* divide success */
      quotient = target / divider;
      if (quotient == 1) {
        /* last */
      } else {
      }
      target = quotient;
      divider = 2;
    } else {
      divider++;
    }
  }
}

void factorization(int target, int id) {
  int divider;
  int quotient;
  int remainder;

  /* edge cases */
  if (target == 0 || target == 1 || target == -1) {
    printf("trial-%d: %d = %d\n", id, target, target);
  }

  if (target < 0) {
    printf("trial-%d: %d = -", id, target);
    target = -target;
  } else {
    printf("trial-%d: %d = ", id, target);
  }

  divider = 2;
  while (target > 1) {
    remainder = target % divider;
    if (remainder == 0) {
      /* divide success */
      quotient = target / divider;
      if (quotient == 1) {
        /* last */
        printf("%d\n", divider);
      } else {
        printf("%d * ", divider);
      }
      target = quotient;
      divider = 2;
    } else {
      divider++;
    }
  }
}

double time_check(int target, int id) {
  /*
   * Check real-runtime of factorization in micro seconds
   *
   * reference
   *  https://stackoverflow.com/questions/5248915/execution-time-of-c-program
   */
  struct timeval start, end;
  double elapsed_time;

  gettimeofday(&start, NULL);
  factorization(target, id);
  gettimeofday(&end, NULL);

  elapsed_time = (double) (end.tv_usec - start.tv_usec) / 1000000 + (double) (end.tv_sec - start.tv_sec);

  /* in ms unit */
  elapsed_time *= 1000.;

  return elapsed_time;
}

void dummy_task(void) {
  /* simple job loop */
  for(int i = 0; i < ITER; i++) {
    factorization_no_print(TARGET);
  }
}

void set_sched_wrr(pid_t pid) {
  /*
   * sched_setscheduler wrr wrapper
   */
  struct sched_param dummy;
  int syscallResult;

  syscallResult = sched_setscheduler(pid, SCHED_WRR, &dummy);
  if (syscallResult == -1) {
    printf("Error! exit...\n");
    exit(1);
  }
}

void set_wrr_weight(pid_t pid, int weight) {
  /*
   * sched_set_weight wrapper
   */
  int syscallResult;
  syscallResult = syscall(SET_WEIGHT_NUM, 0, weight);
  if (syscallResult == -1) {
    printf("Error in set weight! exit...\n");
    exit(1);
  }
}

void get_each_weight(int *weights) {
  int syscallResult;
  syscallResult = syscall(GET_EACH_WEIGHT, weights, CPUS);
  if (syscallResult == -1) {
    printf("Error in get each weight! exit...\n");
    exit(1);
  }
}

double elapsed_time(struct timeval *from, struct timeval *to) {
  /* calculate elapsed time in ms unit */
  double elapsed;

  elapsed = (double) (to->tv_usec - from->tv_usec) / 1000000 + (double) (to->tv_sec - from->tv_sec);
  return elapsed * 1000;
}

#endif
