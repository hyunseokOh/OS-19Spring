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

#define NUM_TEST 10

void do_work(void) {
  for (int i = 0; i < 500; i++) factorization_no_print(TARGET);
}

int main(int argc, char *argv[]) {
  srand(time(NULL));
  int nproc;
  struct timeval start, end;
  pid_t *pids;
  FILE *fpr;
  char filename[80];
  int status = 0;
  double elapsed = 0;
  int target_weight = 0;

  if (argc != 2) {
    return -1;
  }

  nproc = atoi(argv[1]);
  pids = (pid_t *) malloc(sizeof(pid_t) * nproc);

  set_sched_wrr(0);
  snprintf(filename, sizeof(filename), "random_%d.txt", nproc);
  fpr = fopen(filename, "w");
  for (int w = 1; w <= 20; w++) {
    /* calculate for each weight */
    for (int k = 0; k < NUM_TEST; k++) {
      for (int i = 0; i < nproc; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
          if (i == nproc - 1) {
            set_wrr_weight(0, w);
            gettimeofday(&start, NULL);
            do_work();
            gettimeofday(&end, NULL);
            elapsed = (double) (end.tv_usec - start.tv_usec) / 1000000 + (double) (end.tv_sec - start.tv_sec);
            elapsed *= 1000.;

            /*printf("Weight = %d, nproc = %d, elapsed time = %.10f [ms]\n", w, nproc, elapsed);*/
            if (k == NUM_TEST - 1) {
              fprintf(fpr, "%.6f\n", elapsed);
            } else {
              fprintf(fpr, "%.6f\t", elapsed);
            }
            return 0;
          } else {
            set_wrr_weight(0, rand() % 20 + 1);
            do_work();
            return 0;
          }
        }
      }
      while (wait(&status) > 0);
    }
    printf("Weight %d finished\n", w);
  }
  fclose(fpr);
  free(pids);
}
