#include <linux/syscalls.h>
#include <linux/gps.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(gps_lock); /* for accessing shared data */
struct gps_location gps_loc; // TODO: what would be the proper initial value for gps_loc?

int valid_gps_location(struct gps_location *loc) {
  return valid_longitude(loc->lng_integer) &&
    valid_latitude(loc->lat_integer) &&
    valid_fractional(loc->lat_fractional) &&
    valid_fractional(loc->lng_fractional);
}

/* need to implement a function to get current gps_location
 * since, set_gps_location in inode does not receive gps_location as its argument
 */
struct gps_location get_current_gps_location() {
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
  gps_loc = kloc;
  mutex_unlock(&gps_lock);

free_and_return:
  kfree(kloc);
  return retval;
}
