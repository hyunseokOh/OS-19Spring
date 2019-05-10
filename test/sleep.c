/* sleep test */
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

  struct sched_param param = {
    .sched_priority = 0
  };


  pid = getpid();
  int set_result;
  set_result = sched_setscheduler(0, SCHED_WRR, &param);

  /* wrr */
  printf("Set scheduler to %d finished with return %d, errno = %d\n", SCHED_WRR, set_result, errno);
  sleep(2);
  printf("Sleep finished\n");
}
