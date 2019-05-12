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

#define SECONDS 10

void do_work(void) {
  /* loop for 25 seconds */
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
  FILE *fpr;
  char filename[80];
  int status = 0;

  int weights[CPUS];
  double total_weights;
  double balance_factor[CPUS];

  if (argc != 2) {
    return -1;
  }

  nproc = atoi(argv[1]);
  pids = (pid_t *) malloc(sizeof(pid_t) * nproc);
  snprintf(filename, sizeof(filename), "balance_%d.txt", nproc);

  set_sched_wrr(0);
  for (int i = 0; i < nproc; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      if (i == 0) {
        /* first child */
        fpr = fopen(filename, "w");
        gettimeofday(&start, NULL);
        checkpoint.tv_sec = start.tv_sec;
        while(1) {
          gettimeofday(&end, NULL);
          /* every 0.5 seconds */
          if (end.tv_sec - start.tv_sec > SECONDS) break;
          if ((double) (end.tv_sec - checkpoint.tv_sec) > 0.5) {
            checkpoint.tv_sec = end.tv_sec;
            get_each_weight(weights);
            total_weights = 0;
            for (int j = 0; j < CPUS; j++) {
              balance_factor[j] = 0;
              total_weights += (double) weights[j];
            }

            for (int j = 0; j < CPUS; j++) {
              balance_factor[j] = (double) weights[j] / total_weights * 100.;
              if (j == CPUS - 1) {
                fprintf(fpr, "%.6f\n", balance_factor[j]);
              } else {
                fprintf(fpr, "%.6f\t", balance_factor[j]);
              }
            }
          }
        }
        fclose(fpr);
        return 0;
      } else {
        do_work();
        return 0;
      }
    }
  }
  while (wait(&status) > 0);
  free(pids);
}
