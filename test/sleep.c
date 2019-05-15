/* sleep test */
#include "trial.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  pid_t pid;

  pid = getpid();
  set_sched_wrr(pid);

  /* wrr */
  printf("Now in SCHED_WRR, sleep for 2 seconds...\n");
  sleep(2);
  printf("Sleep finished\n");
}
