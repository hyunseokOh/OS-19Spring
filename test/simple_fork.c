#include "trial.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>

#define SET_WEIGHT_NUM 398
#define GET_WEIGHT_NUM 399
#define SCHED_WRR 7

int main(void) {
  struct sched_param param = {
    .sched_priority = 0
  };
  int syscallResult;
  syscallResult = sched_setscheduler(0, SCHED_WRR, &param);
  printf("set schedule result = %d\n", syscallResult);
  syscallResult = syscall(SET_WEIGHT_NUM, 0, 1);
  printf("set weight result = %d\n", syscallResult);
  for (int i = 0; i < 3; i++) {
    fork();
    factorization(1000000001, 0);
  }
}
