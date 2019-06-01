#include <linux/syscalls.h>
#include <linux/gps.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/kfloat.h>
#include <linux/printk.h>

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(gps_lock); /* for accessing shared data */
struct gps_location gps_loc = { 0, 0, 0, 0, 0 };

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
  /*
   * no strlen_user is in tizen?
   * From elixir, strlen_user is defined as 
   * static inline long strlen_user(const char __user *src)
   * {
   *  return strnlen_user(src, 32767);
   * }
   * so, let's take 32767 and use strnlen_user
   */
  long path_length = strnlen_user(pathname, 32767);
  char *kpathname;
  struct gps_location kloc;
  struct inode *inode;
  int retval = 0;

  if (path_length == 0) {
    /* return by !access_ok in strnlen_user */
    retval = -EINVAL;
    goto return_value;
  }

  kpathname = (char *)kmalloc(path_length * sizeof(char), GFP_KERNEL);
  if (kpathname == NULL) {
    retval = -ENOMEM;
    goto return_value;
  }

  retval = strncpy_from_user(kpathname, pathname, path_length);
  if (retval < 0) {
    /* EFAULT */
    goto free_pathname;
  }

  inode = get_inode(kpathname, &retval);
  if (retval < 0) {
    retval = -ENOENT;
    goto free_pathname;
  }

  retval = generic_permission(inode, MAY_READ);
  if (retval < 0) {
    /* must be EACCESS */
    goto free_pathname;
  }

  if (inode->i_op->get_gps_location) {
    retval = inode->i_op->get_gps_location(inode, &kloc);
    if (retval < 0) {
      /* cannot access (EACCES) */
      goto free_pathname;
    }
  } else {
    /*
     * FIXME (taebum)
     * Can we say there is no GPS coordinate if get_gps_location is NULL?
     */
    retval = -ENODEV;
    goto free_pathname;
  }

  if (copy_to_user(loc, &kloc, sizeof(struct gps_location))) {
    retval = -EFAULT;
    goto free_pathname;
  }

free_pathname:
  kfree(kpathname);
return_value:
  return retval;
}

static inline void print_kfloat_info(const kfloat *f) {
  /* for debugging */
  printk("GONGLE: value = %lld, pos = %d\n", VAL(f), POS(f));
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
