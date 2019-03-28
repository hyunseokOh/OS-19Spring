#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTUNLOCK_READ 401

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int trialRun = 1;

void intHandler(int sig) {
  /* handle ctrl-c interrupt */
  trialRun = 0;
}

void factorization(int target, int id) {
  int divider;
  int quotient;
  int remainder;

  /* edge cases */
  if (target == 0 || target == 1 || target == -1) {
    printf("trial-%d: %d = %d", id, target, target);
  }

  if (target < 0) {
    printf("trial-%d: %d = -", id, target);
    target = -target;
  } else {
    printf("trial-%d: %d = ", id, target);
  }

  divider = 2;
  while (target > 1) {
    remainder = target % divider;
    if (remainder == 0) {
      /* divide success */
      quotient = target / divider;
      if (quotient == 1) {
        /* last */
        printf("%d\n", divider);
      } else {
        printf("%d * ", divider);
      }
      target = quotient;
      divider = 2;
    } else {
      divider++;
    }
  }
}

int main(int argc, char *argv[]) {
  int target;
  int id;
  long long_id;
  FILE *fp = NULL;
  char *endPtr = NULL;

  if (argc < 2) {
    printf("Must pass id number of trial\n");
    return 0;
  }

  long_id = strtol(argv[1], &endPtr, 10);

  if (*endPtr) {
    /* endPtr points to NULL char only when the entire argument is a number */
    printf("INVALID ARGUMENT: invalid character exists or no digits at all\n");
    return 0;
  } else if (errno != 0) {
    /* errno is set to ERANGE if underflow or overflow occurs */
    printf("INVALID ARGUMENT: most likely overflow or underflow\n");
    return 0;
  } else if ((long_id > INT_MAX) || (long_id < INT_MIN)) {
    printf("INVALID ARGUMENT: number cannot be expressed as int'\n");
    return 0;
  } else {
    id = (int)long_id;
  }

  signal(SIGINT, intHandler);
  fp = fopen("integer", "r");

  while (trialRun) {
    /* TODO (taebum) require read lock before read */
    fscanf(fp, "%d\n", &target);
    factorization(target, id);
    /* TODO (taebum) read lock release */
    rewind(fp);

    sleep(2);
  }

  fclose(fp);
}
