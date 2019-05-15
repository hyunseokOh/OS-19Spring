#include "trial.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  set_sched_wrr(0);
  set_wrr_weight(0, 1);
  for (int i = 0; i < 3; i++) {
    fork();
    factorization(1000000001, 0);
  }
}
