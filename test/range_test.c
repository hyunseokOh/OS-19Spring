#include <stdio.h>

#define RANGE_OFFSET (10)
#define ZERO_RANGE_MASK (~(511 << RANGE_OFFSET))
#define ONE_RANGE_MASK (511 << RANGE_OFFSET)

#define GET_LOW(range)                                      \
  ((*(range)&1) == 1 ? -((ZERO_RANGE_MASK & *(range)) >> 1) \
                     : (ZERO_RANGE_MASK & *(range)) >> 1)
#define GET_HIGH(range) (GET_LOW(range) + (*(range) >> RANGE_OFFSET))
#define SET_LOW(range, value)                                      \
  (*(range) = ((value > 0) ? (value << 1) : ((-value) << 1) | 1) | \
              (*(range)&ONE_RANGE_MASK))
#define SET_RANGE(range, value) \
  (*(range) = (ZERO_RANGE_MASK & *(range)) | (value << RANGE_OFFSET))
#define LOW(degree, range) (degree - range)
#define RANGE(range) (range << 1)

int main(void) {
  int degree;
  int range;
  int test;
  int low;
  int high;
  int iter = 0;

  for (degree = 0; degree < 360; degree++) {
    for (range = 1; range < 180; range++) {
      low = degree - range;
      high = degree + range;
      SET_LOW(&test, LOW(degree, range));
      SET_RANGE(&test, RANGE(range));
      if (GET_LOW(&test) == low && GET_HIGH(&test) == high) {
        printf("[%d] Success (degree = %d, range = %d)\n", iter, degree, range);
      } else {
        printf("[%d] Failed (degree = %d, range = %d)\n", iter, degree, range);
        return 0;
      }
      iter++;
    }
  }
  return 0;
}
