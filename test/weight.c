#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <signal.h>
#include "trial.h"

#define NUM_TEST 5


int main(void) {
  double time[20];
  double check;
  for (int i = 0; i < 20; i++) time[i] = 0;
  set_sched_wrr(0);

  for(int i = 1; i <= 20; i++) {
    check = 0;
    set_wrr_weight(0, i);
    for (int t = 0; t < NUM_TEST; t++) {
      check += time_check(TARGET, i);
    }
    time[i - 1] = check / (double) NUM_TEST;
  }

  printf("Time Avg\n");
  for(int i = 0; i < 20; i++) {
    printf("Weight = %d, time = %.10f\n", i + 1, time[i]);
  }
}
