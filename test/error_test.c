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
#define SUCCESS printf("Test %d Success\n", test_num++)

int main(void) {
  int syscallResult;
  int test_num = 1;
  struct sched_param dummy_param;
  cpu_set_t mask;
  pid_t pid;

  /* get weight with negative pid number */
  syscallResult = syscall(GET_WEIGHT_NUM, -1);
  assert(syscallResult == -1);
  assert(errno == 22);
  SUCCESS;

  /* get weight with non-existing pid number */
  syscallResult = syscall(GET_WEIGHT_NUM, INT_MAX);
  assert(syscallResult == -1);
  assert(errno == 3);

  /* get weight of current */
  syscallResult = syscall(GET_WEIGHT_NUM, 0);
  assert(syscallResult == 10);
  SUCCESS;

  /* set weight with non-existing pid number */
  syscallResult = syscall(SET_WEIGHT_NUM, INT_MAX, 10);
  assert(syscallResult == -1);
  assert(errno == 3);

  /* set weight with negative pid */
  syscallResult = syscall(SET_WEIGHT_NUM, -1, 10);
  assert(syscallResult == -1);
  assert(errno == 22);
  SUCCESS;

  /* set weight with invalid weight */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 0);
  assert(syscallResult == -1);
  assert(errno == 22);
  SUCCESS;

  /* set weight whose policy is not WRR (maybe permission error could occur) */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 20);
  assert(syscallResult == -1);
  assert(errno == 22 || errno == 1);
  SUCCESS;

  /* set affinity to only cpu number 3 (forbidden) */
  CPU_ZERO(&mask);
  CPU_SET(3, &mask);
  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
  assert(sched_getcpu() == 3);
  SUCCESS;

  /* set sched as wrr to only cpu number 3 allowed task fail */
  syscallResult = sched_setscheduler(0, SCHED_WRR, &dummy_param);
  assert(syscallResult == -1);
  assert(errno == 22);
  assert(sched_getcpu() == 3);
  SUCCESS;

  /* allow all cpus */
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  CPU_SET(1, &mask);
  CPU_SET(2, &mask);
  CPU_SET(3, &mask);
  syscallResult = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
  assert(syscallResult == 0);
  assert(sched_getcpu() == 3);
  SUCCESS;

  /* change to sched_wrr, not in cpu number 3 */
  syscallResult = sched_setscheduler(0, SCHED_WRR, &dummy_param);
  assert(syscallResult == 0);
  assert(sched_getcpu() != 3);
  SUCCESS;

  /* successfully changed to sched_wrr */
  syscallResult = sched_getscheduler(0);
  assert(syscallResult == SCHED_WRR);
  SUCCESS;

  /* modify weight (decrease) */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 5);
  assert(syscallResult == 0);
  SUCCESS;

  /* check weight modified */
  syscallResult = syscall(GET_WEIGHT_NUM, 0);
  assert(syscallResult == 5);
  SUCCESS;

  /* fork test */
  pid = fork();
  if (pid == 0) {
    /* child */
    /* must copy parent's weight */
    syscallResult = syscall(GET_WEIGHT_NUM, 0);
    assert(syscallResult == 5);
    SUCCESS;
  } else {
    /* parent */
    /* modify weight (increase) */
    syscallResult = syscall(SET_WEIGHT_NUM, 0, 20);
    if (syscallResult == -1) {
      /* error iff user try to increase */
      printf("Cannot increase weight (user)\n");
      assert(errno == 1);
      SUCCESS;
    } else {
      syscallResult = syscall(GET_WEIGHT_NUM, 0);
      assert(syscallResult == 20);
      SUCCESS;
    }
    /* invalid weight set */
    syscallResult = syscall(SET_WEIGHT_NUM, 0, 22);
    assert(syscallResult == -1);
    assert(errno == 22);
    SUCCESS;

    printf("All errror test passed\n");
  }
}
