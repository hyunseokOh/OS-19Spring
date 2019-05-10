#ifndef _TRIAL_H
#define _TRIAL_H

#include <stdio.h>

void factorization(int target, int id) {
  int divider;
  int quotient;
  int remainder;

  /* edge cases */
  if (target == 0 || target == 1 || target == -1) {
    printf("trial-%d: %d = %d\n", id, target, target);
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

#endif
