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

  printf("All errror test passed\n");
}
