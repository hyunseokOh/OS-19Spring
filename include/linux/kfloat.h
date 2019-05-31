#ifndef _LINUX_KFLOAT_H
#define _LINUX_KFLOAT_H

#define VAL(f) (f->value)
#define POS(f) (f->pos)
#define RADIAN(f) (kfloat_mul(f, &degree2rad))
#define KINT(f) (VAL(f) / safe_pad(POS(f)))
#define KFRAC(f) (VAL(f) % safe_pad(POS(f)))

typedef struct {
  long long value;
  int pos;
} kfloat;

/* useful constants */
extern long long pad_idx[19];
extern kfloat factorials[20];
extern kfloat degree90;
extern kfloat degree180;
extern kfloat degree180_neg;
extern kfloat degree360;
extern kfloat degree90;
extern kfloat earth_radius;
extern kfloat degree2rad;
extern kfloat rad2degree;
extern kfloat constant_1;

static inline int overflow_add(long long l1, long long l2) {
  /*
   * Check whether addition incurs overflow (underflow)
   */
  long long result = l1 + l2;
  if (l1 > 0 && l2 > 0 && result < 0) {
    return 1;
  } else if(l1 < 0 && l2 < 0 && result > 0) {
    return 1;
  } else {
    return 0;
  }
}

static inline int overflow_sub(long long l1, long long l2) {
  /*
   * Check whether subtraction incurs overflow (underflow)
   */
  long long result = l1 + l2;
  if (l1 > 0 && l2 < 0 && result < 0) {
    return 1;
  } else if(l1 < 0 && l2 > 0 && result > 0) {
    return 1;
  } else {
    return 0;
  }
}

static inline int overflow_mul(long long l1, long long l2) {
  /*
   * Check whether multiplication incurs overflow (underflow)
   */
  long long result;

  if (l1 == 0 || l2 == 0) {
    /* prevent zero divide */
    return 0;
  }

  result = l1 * l2;

  if (l1 == result / l2) {
    return 0;
  } else {
    return 1;
  }
}

static inline void truncate(kfloat *f) {
  /*
   * truncate floating point
   *  - redundant zero
   *  - if pos > 18, remove the last one
   */
  while ((VAL(f) % 10 == 0 && POS(f) > 0) || POS(f) > 18) {
    f->value = f->value / 10;
    f->pos = f->pos - 1;
  }
}

int kfloat_comp(const kfloat *f1, const kfloat *f2);
kfloat kfloat_add(const kfloat *f1, const kfloat *f2);
kfloat kfloat_sub(const kfloat *f1, const kfloat *f2);
kfloat kfloat_mul(const kfloat *f1, const kfloat *f2);
kfloat kfloat_sin(const kfloat *f);
kfloat kfloat_cos(const kfloat *f);
kfloat kfloat_neg(const kfloat *f);
kfloat to_kfloat(int integer, int fraction);



#endif
