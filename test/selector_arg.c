#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_WRITE 402

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int selectorRun = 1;

void intHandler(int sig) {
  /* handle ctrl-c interrupt */
  selectorRun = 0;
}

int main(int argc, char *argv[]) {
  int num;
  long longNum;
  FILE *fp = NULL;
  char *endPtr = NULL;

  if (argc != 4) {
    printf("Example Usage: ./selector [starting num] [degree] [range] \n");
    return 0;
  }

  longNum = strtol(argv[1], &endPtr, 10);
  /* TODO:(seonghoon) change depreciated atoi() to strtol with error-handling */
  int degree = atoi(argv[2]);
  int range = atoi(argv[3]);


  if (*endPtr) {
    /* endPtr points to NULL char only when the entire argument is a number */
    printf("INVALID ARGUMENT: invalid character exists or no digits at all\n");
    return 0;
  } else if (errno != 0) {
    /* errno is set to ERANGE if underflow or overflow occurs */
    printf("INVALID ARGUMENT: most likely overflow or underflow\n");
    return 0;
  } else if ((longNum > INT_MAX) || (longNum < INT_MIN)) {
    printf("INVALID ARGUMENT: number cannot be expressed as int'\n");
    return 0;
  } else {
    num = (int)longNum;
  }

  signal(SIGINT, intHandler);

  while (selectorRun) {
//    syscall(SYSCALL_ROTLOCK_WRITE, 90, 90);
    syscall(SYSCALL_ROTLOCK_WRITE, degree, range);
    fp = fopen("integer", "w");
    if (fp != NULL) {
      fprintf(fp, "%d\n", num);
      printf("selector: %d\n", num++);
      fclose(fp);
      fp = NULL;
    }
//    syscall(SYSCALL_ROTUNLOCK_WRITE, 90, 90);
    syscall(SYSCALL_ROTUNLOCK_WRITE, degree, range);
  }

  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }
}
