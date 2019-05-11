/* io wait test*/
#include "trial.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  /* simple i/o response test */
  pid_t pid;
  pid = getpid();
  set_sched_wrr(pid);

  /* wrr */
  printf("Now in SCHED_WRR, waiting for char...");
  getchar();
}
