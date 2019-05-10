#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>

#define SET_WEIGHT_NUM 398
#define GET_WEIGHT_NUM 399
#define SCHED_WRR 7

int main(void) {
  int syscallResult;
  struct sched_param dummy_param;
  pid_t pid;

  /* get weight with negative number */
  syscallResult = syscall(GET_WEIGHT_NUM, -1);
  assert(syscallResult == -1);
  assert(errno == 22);

  /* get weight of current */
  syscallResult = syscall(GET_WEIGHT_NUM, 0);
  assert(syscallResult == 10);

  /* set weight with negative pid */
  syscallResult = syscall(SET_WEIGHT_NUM, -1, 10);
  assert(syscallResult == -1);
  assert(errno == 22);

  /* set weight with invalid weight */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 0);
  assert(syscallResult == -1);
  assert(errno == 22);

  /* set weight whose policy is not WRR */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 20);
  assert(syscallResult == -1);
  assert(errno == 22);

  /* change to sched_wrr */
  syscallResult = sched_setscheduler(0, SCHED_WRR, &dummy_param);
  assert(syscallResult == 0);

  /* successfully changed to sched_wrr */
  syscallResult = sched_getscheduler(0);
  assert(syscallResult == SCHED_WRR);

  /* modify weight (decrease) */
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 5);
  assert(syscallResult == 0);

  /* check weight modified */
  syscallResult = syscall(GET_WEIGHT_NUM, 0);
  assert(syscallResult == 5);

  /* fork test */
  pid = fork();
  if (pid == 0) {
    /* child */
    /* must copy parent's weight */
    syscallResult = syscall(GET_WEIGHT_NUM, 0);
    assert(syscallResult == 5);
  } else {
    /* parent */
    /* modify weight (increase) */
    syscallResult = syscall(SET_WEIGHT_NUM, 0, 20);
    if (syscallResult == -1) {
      /* error iff user try to increase */
      printf("Cannot increase weight\n");
      assert(errno == 1);
    } else {
      syscallResult = syscall(GET_WEIGHT_NUM, 0);
      assert(syscallResult == 20);
    }
    /* invalid weight set */
    syscallResult = syscall(SET_WEIGHT_NUM, 0, 22);
    assert(syscallResult == -1);
    assert(errno == 22);
    printf("All errror test passed\n");
  }
}
