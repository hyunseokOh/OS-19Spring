/*
 * Test for 2 write locks with intersection
 * (must be crossed)
 */
#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_READ 401
#define SYSCALL_ROTUNLOCK_WRITE 402
#define N_THREAD 2

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

int global_value = 0;
int INCREMENT = 1000;
int verbose = 0;

void *increment_global(void *arg) {
  int i;

  for (i = 0; i < INCREMENT; i++) {
    /* 10 <= range <= 60 */
    syscall(SYSCALL_ROTLOCK_WRITE, 30, 40);
    global_value++;
    if (verbose) printf("Global value from increment = %d\n", global_value);
    syscall(SYSCALL_ROTUNLOCK_WRITE, 30, 40);
  }
}

void *decrement_global(void *arg) {
  int i;

  for (i = 0; i < INCREMENT; i++) {
    /* 300 <= range <= 400 (40) */
    syscall(SYSCALL_ROTLOCK_WRITE, 350, 50);
    global_value--;
    if (verbose) printf("Global value from decrement = %d\n", global_value);
    syscall(SYSCALL_ROTUNLOCK_WRITE, 350, 50);
  }
}

int main(int argc, char *argv[]){
  char *endPtr;
  INCREMENT = strtol(argv[1], &endPtr, 10);
  verbose = strtol(argv[2], &endPtr, 10);
  pthread_t threads[N_THREAD];
  pthread_create(&threads[0], NULL, &increment_global, NULL);
  pthread_create(&threads[1], NULL, &decrement_global, NULL);
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
  printf("Result global value = %d\n (Should be 0!)\n", global_value);
}
