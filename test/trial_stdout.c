/*
 * main test function
 */
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <wait.h>
#include "trial.h"

#define NUM_TEST 1

void do_work(void) {
  for (int i = 0; i < 500; i++) factorization_no_print(TARGET);
}

int main(int argc, char *argv[]) {
  int nproc;
  struct timeval start, end;
  pid_t *pids;
  int status = 0;
  double elapsed = 0;
  int equal_weight = 0;

  if (argc != 2) {
    if (argc == 3) {
      equal_weight = atoi(argv[2]);
    } else {
      return -1;
    }
  }

  nproc = atoi(argv[1]);
  pids = (pid_t *) malloc(sizeof(pid_t) * nproc);

  set_sched_wrr(0);
  for (int w = 1; w <= 20; w++) {
    /* calculate for each weight */
    if (equal_weight) {
      /* all same weights */
      set_wrr_weight(0, w);
    }
    for (int k = 0; k < NUM_TEST; k++) {
      for (int i = 0; i < nproc; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
          if (i == nproc - 1) {
            if (!equal_weight) {
              set_wrr_weight(0, w);
            }
            gettimeofday(&start, NULL);
            do_work();
            gettimeofday(&end, NULL);
            elapsed = (double) (end.tv_usec - start.tv_usec) / 1000000 + (double) (end.tv_sec - start.tv_sec);
            elapsed *= 1000.;

            printf("Weight = %d, nproc = %d, elapsed time = %.6f [ms]\n", w, nproc, elapsed);
            return 0;
          } else {
            do_work();
            return 0;
          }
        }
      }
      while (wait(&status) > 0);
    }
  }
  free(pids);
}
