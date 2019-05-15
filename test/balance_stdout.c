/*
 * main test function
 * random weight distribution
 */
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <wait.h>
#include "trial.h"

#define SECONDS 8

void do_work(void) {
  /* loop for SECONDS */
  struct timeval start, end;
  gettimeofday(&start, NULL);
  while (1) {
    gettimeofday(&end, NULL);
    if (end.tv_sec - start.tv_sec > SECONDS) return;
  }
}

int main(int argc, char *argv[]) {
  srand(time(NULL));
  int nproc;
  struct timeval start, end, checkpoint;
  pid_t *pids;
  int status = 0;

  int weights[CPUS];
  double total_weights;
  double balance_factor[CPUS];
  double elapsed;

  if (argc != 2) {
    return -1;
  }

  nproc = atoi(argv[1]);
  pids = (pid_t *) malloc(sizeof(pid_t) * nproc);

  for (int i = 0; i < nproc + 1; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      if (i == 0) {
        /* first child */
        gettimeofday(&start, NULL);
        checkpoint.tv_sec = start.tv_sec;
        checkpoint.tv_usec = start.tv_usec;
        while(1) {
          gettimeofday(&end, NULL);
          if (end.tv_sec - start.tv_sec > SECONDS) break;
          /* every seconds (safely) */

          elapsed = (double) (end.tv_usec - checkpoint.tv_usec) / 1000000 + (double) (end.tv_sec - checkpoint.tv_sec);
          elapsed *= 1000;
          if (elapsed > 200) {
            /* at every 0.2 seconds */
            checkpoint.tv_sec = end.tv_sec;
            checkpoint.tv_usec = end.tv_usec;
            get_each_weight(weights);
            total_weights = 0;
            for (int j = 0; j < CPUS; j++) {
              balance_factor[j] = 0;
              total_weights += (double) weights[j];
            }

            for (int j = 0; j < CPUS; j++) {
              balance_factor[j] = (double) weights[j] / total_weights * 100.;
              if (j == CPUS - 1) {
                printf("%.6f [CPU %d]\n", balance_factor[j], j);
              } else {
                printf("%.6f [CPU %d]\t", balance_factor[j], j);
              }
            }
          }
        }
        return 0;
      } else {
        do_work();
        return 0;
      }
    } else {
      if (i == 0) {
        set_sched_wrr(0);
      }
    }
  }
  while (wait(&status) > 0);
  free(pids);
}
