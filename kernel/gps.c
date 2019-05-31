#include <linux/syscalls.h>
#include <linux/gps.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/kfloat.h>

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(gps_lock); /* for accessing shared data */
struct gps_location gps_loc; // TODO: what would be the proper initial value for gps_loc?

int valid_gps_location(struct gps_location *loc) {
  return valid_longitude(loc->lng_integer) &&
    valid_latitude(loc->lat_integer) &&
    valid_fractional(loc->lat_fractional) &&
    valid_fractional(loc->lng_fractional) &&
    valid_accuracy(loc->accuracy);
}

/* need to implement a function to get current gps_location
 * since, set_gps_location in inode does not receive gps_location as its argument
 */
struct gps_location get_current_gps_location(void) {
  struct gps_location loc;

  mutex_lock(&gps_lock);
  loc = gps_loc;
  mutex_unlock(&gps_lock);

  return loc;
}

SYSCALL_DEFINE1(set_gps_location, struct gps_location __user *, loc) {
  struct gps_location *kloc;
  int retval = 0;

  kloc = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);

  if (kloc == NULL) {
    return -ENOMEM;
  }

  if (copy_from_user(kloc, loc, sizeof(struct gps_location))) {
    retval = -EFAULT;
    goto free_and_return;
  }

  if (!valid_gps_location(kloc)) {
    retval = -EINVAL;
    goto free_and_return;
  }

  /* grab lock before updating gps_location */
  mutex_lock(&gps_lock);
  gps_loc = *kloc;
  mutex_unlock(&gps_lock);

free_and_return:
  kfree(kloc);
  return retval;
}

SYSCALL_DEFINE2(get_gps_location, const char __user *, pathname, struct gps_location __user *, loc) {
  return 0;
}

static inline kfloat _access_range(int acc1, int acc2) {
  kfloat facc1 = { (long long) acc1, 3 }; /* m to km */
  kfloat facc2 = { (long long) acc2, 3 }; /* m to km */
  kfloat result = kfloat_add(&facc1, &facc2);

  result = kfloat_mul(&result, &earth_radius);
  result = kfloat_mul(&result, &rad2degree);

  return result;
}

int can_access(struct gps_location *g1, struct gps_location *g2) {
  kfloat g1_lat;
  kfloat g1_lng;
  kfloat g2_lat;
  kfloat g2_lng;

  kfloat lat_sub;
  kfloat lng_sub;
  kfloat tmp;

  kfloat distance;
  kfloat access_range;
  int result;

  if (g1->accuracy + g2->accuracy > 20037000) {
    /* cover 180 degree (always access) */
    return 1;
  }

  g1_lat = to_kfloat(g1->lat_integer, g1->lat_fractional);
  g1_lng = to_kfloat(g1->lng_integer, g1->lng_fractional);
  g2_lat = to_kfloat(g2->lat_integer, g2->lat_fractional);
  g2_lng = to_kfloat(g2->lng_integer, g2->lng_fractional);

  lat_sub = kfloat_sub(&g1_lat, &g2_lat);
  lng_sub = kfloat_sub(&g1_lng, &g2_lng);

  access_range = _access_range(g1->accuracy, g2->accuracy);
  access_range = kfloat_cos(&access_range);

  g1_lat = kfloat_cos(&g1_lat);
  g2_lat = kfloat_cos(&g2_lat);

  tmp = kfloat_cos(&lng_sub);
  tmp = kfloat_sub(&constant_1, &tmp);
  tmp = kfloat_mul(&g2_lat, &tmp);
  tmp = kfloat_mul(&g1_lat, &tmp);

  distance = kfloat_cos(&lat_sub);
  distance = kfloat_sub(&distance, &tmp);

  result = kfloat_comp(&distance, &access_range);

  if (result < 0) {
    return 0;
  } else {
    return 1;
  }
}
