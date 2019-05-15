#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>

#define SET_WEIGHT_NUM 398
#define GET_WEIGHT_NUM 399
#define SCHED_WRR 7

int main(void) {
  int syscallResult;
  pid_t pid;
  struct sched_param dummy;

  struct sched_param param = {
    .sched_priority = 0
  };

  printf("Change policy into SCHED_WRR...\n");
  sched_setscheduler(0, SCHED_WRR, &param);

  printf("Change weight as 20...\n");
  syscall(SET_WEIGHT_NUM, 0, 20);

  printf("sched_get_weight result = %ld\n", syscall(GET_WEIGHT_NUM, 0));
}
