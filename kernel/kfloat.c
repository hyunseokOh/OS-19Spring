/*
 * Kernel Floating Point Implementation
 */
#include <linux/kfloat.h>
#include <linux/kernel.h>

/* pos padding table */
long long pad_idx[19] = {
  1e+0, 1e+1, 1e+2, 1e+3, 1e+4,
  1e+5, 1e+6, 1e+7, 1e+8, 1e+9,
  1e+10, 1e+11, 1e+12, 1e+13, 1e+14,
  1e+15, 1e+16, 1e+17, 1e+18
};

/* factorial division table */
kfloat factorials[20] = {
  { 1, 0 },
  { 1, 0 },
  { 5, 1 },
  { 166666666666666667, 18 },
  { 41666666666666667, 18 },
  { 8333333333333333, 18 },
  { 1388888888888889, 18 },
  { 198412698412698, 18 },
  { 24801587301587, 18 },
  { 2755731922399, 18 },
  { 27557319224, 17 },
  { 25052108385, 18 },
  { 2087675699, 18},
  { 160590438, 18 },
  { 11470746, 18},
  { 764716, 18},
  { 47795, 18},
  { 2811, 18},
  { 156, 18},
  { 8, 18},
};

kfloat degree2rad =   { 17453292519943295, 18 };
kfloat rad2degree =   { 572957795130823228, 16 };
kfloat degree90   =   { 90, 0 };
kfloat degree180  =   { 180, 0 };
kfloat degree180_neg =  { -180, 0 };
kfloat degree360     =  { 360, 0 };
kfloat earth_radius = { 156788962057071, 18 };
kfloat constant_1 = { 1, 0 };

kfloat kfloat_neg(const kfloat *f) {
  /*
   * Safe negation considering over(under)flow
   * We cannot negate LLONG_MIN
   * In that case, just convert to LLONG_MAX
   */
  kfloat result;
  if (VAL(f) == LLONG_MIN) {
    result.value = LLONG_MAX;
  } else {
    result.value = -VAL(f);
  }

  result.pos = POS(f);
  return result;
}

int kfloat_comp(const kfloat *f1, const kfloat *f2) {
  /*
   * Compare two floats
   * f1 < f2: return -1
   * f1 == f2: return 0
   * f1 > f2: return 1
   *
   * actually, similar as subtraction
   */

  long long f1_padded;
  long long f2_padded;
  long long sub;

  if (POS(f1) > POS(f2)) {
    f1_padded = VAL(f1);
    f2_padded = VAL(f2) * pad_idx[POS(f1) - POS(f2)];
  } else {
    f1_padded = VAL(f1) * pad_idx[POS(f2) - POS(f1)];
    f2_padded = VAL(f2);
  }

  sub = f1_padded - f2_padded;
  if (sub < 0) {
    return -1;
  } else if (sub > 0) {
    return 1;
  } else {
    return 0;
  }
}

static inline kfloat _kfloat_add(const kfloat *f1, const kfloat *f2) {
  /*
   * Assumption: POS(f1) >= POS(f2) always
   */
  int pad;
  int pos;
  long long f1_val;
  long long f2_val;
  kfloat result;

  pad = POS(f1) - POS(f2);
  pos = POS(f1);
  f1_val = VAL(f1);

  while (overflow_mul(VAL(f2), pad_idx[pad]) && pad > 0) {
    f1_val /= 10;
    pad--;
    pos--;
  }

  f2_val = VAL(f2) * pad_idx[pad];

  while (overflow_add(f1_val, f2_val) && pos > 0) {
    f1_val /= 10;
    f2_val /= 10;
    pos--;
  }

  result.value = f1_val + f2_val;
  result.pos = pos;

  truncate(&result);

  return result;
}

kfloat kfloat_add(const kfloat *f1, const kfloat *f2) {
  kfloat result;

  if (POS(f1) > POS(f2)) {
    result = _kfloat_add(f1, f2);
  } else {
    result = _kfloat_add(f2, f1);
  }

  return result;
}

kfloat kfloat_sub(const kfloat *f1, const kfloat *f2) {
  kfloat result;
  kfloat neg_f2 = kfloat_neg(f2);

  if (POS(f1) > POS(f2)) {
    result = _kfloat_add(f1, &neg_f2);
  } else {
    result = _kfloat_add(&neg_f2, f1);
  }
  return result;
}

kfloat kfloat_mul(const kfloat *f1, const kfloat *f2) {
  int f1_pos;
  int f2_pos;

  long long f1_val;
  long long f2_val;

  kfloat result;

  f1_pos = POS(f1);
  f2_pos = POS(f2);
  f1_val = VAL(f1);
  f2_val = VAL(f2);

  while (overflow_mul(f1_val, f2_val) && (f1_pos > 0 || f2_pos > 0)) {
    if (f1_pos > f2_pos) {
      f1_val /= 10;
      f1_pos--;
    } else {
      f2_val /= 10;
      f2_pos--;
    }
  }

  result.value = f1_val * f2_val;
  result.pos = f1_pos + f2_pos;

  truncate(&result);

  return result;
}

kfloat kfloat_sin(const kfloat *f) {
  long long f_int;
  kfloat moved;
  kfloat x;
  kfloat x2;
  kfloat x3;
  kfloat x5;
  kfloat x7;
  kfloat x9;
  kfloat x11;
  kfloat x13;
  kfloat x15;
  kfloat result;

  f_int = KINT(f);
  moved = *f;

  if (90 < f_int && f_int <= 180) {
    /* left shift */
    moved = kfloat_sub(&degree180, f);
  } else if (-180 <= f_int && f_int < -90) {
    /* right shift */
    moved = kfloat_sub(&degree180_neg, f);
  }

  x = RADIAN(&moved);
  x2 = kfloat_mul(&x, &x);
  x3 = kfloat_mul(&x, &x2);
  x5 = kfloat_mul(&x3, &x2);
  x7 = kfloat_mul(&x5, &x2);
  x9 = kfloat_mul(&x7, &x2);
  x11 = kfloat_mul(&x9, &x2);
  x13 = kfloat_mul(&x11, &x2);
  x15 = kfloat_mul(&x13, &x2);

  x3 = kfloat_mul(factorials + 3, &x3);
  x5 = kfloat_mul(factorials + 5, &x5);
  x7 = kfloat_mul(factorials + 7, &x7);
  x9 = kfloat_mul(factorials + 9, &x9);
  x11 = kfloat_mul(factorials + 11, &x11);
  x13 = kfloat_mul(factorials + 13, &x13);
  x15 = kfloat_mul(factorials + 15, &x15);

  result = kfloat_sub(&result, &x3);
  result = kfloat_add(&result, &x5);
  result = kfloat_sub(&result, &x7);
  result = kfloat_add(&result, &x9);
  result = kfloat_sub(&result, &x11);
  result = kfloat_add(&result, &x13);
  result = kfloat_sub(&result, &x15);

  return result;
}

kfloat kfloat_cos(const kfloat *f) {
  kfloat moved;
  long long f_int;

  f_int = KINT(f);
  moved = *f;

  /*
   * Invariant: always between (-360 <= f <= 360) in degree
   */
  if (180 < f_int && f_int <= 360) {
    moved = kfloat_sub(f, &degree360);
    f_int -= 360;
  } else if (-360 <= f_int && f_int < -180) {
    moved = kfloat_add(f, &degree360);
    f_int += 360;
  }

  /*
   * Invariant: always between (-180 <= f <= 180) in degree
   */
  if (0 <= f_int && f_int <= 180) {
    /*
     * cos(x) == sin(90 - x)
     */
    moved = kfloat_sub(&degree90, &moved);
  } else {
    /*
     * -180 <= f_int < 0
     *  cos(x) == sin(90 + x)
     */
    moved = kfloat_add(&degree90, &moved);
  }
  return kfloat_sin(&moved);
}

kfloat to_kfloat(int integer, int fraction) {
  /* type casting */
  int pos = 0;
  int frac = fraction;
  long long value;
  kfloat result;

  while (frac > 0) {
    frac /= 10;
    pos++;
  }

  value = integer * pad_idx[pos] + fraction;

  result.value = value;
  result.pos = pos;
  return result;
}
