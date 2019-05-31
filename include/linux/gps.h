#ifndef _LINUX_GPS_H
#define _LINUX_GPS_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kfloat.h>
#include <linux/mutex.h>

/* use in inode.c */
extern struct mutex gps_lock;
extern struct gps_location gps_loc;

struct gps_location {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
};

static inline int valid_longitude(int longitude) {
  return -180 <= longitude && longitude <= 180;
}

static inline int valid_latitude(int latitude) {
  return -90 <= latitude && latitude <= 90;
}

static inline int valid_fractional(int fractional) {
  return 0 <= fractional && fractional <= 999999;
}

static inline int valid_accuracy(int accuracy) {
  return accuracy >= 0;
}

int valid_gps_location(struct gps_location *loc);

int can_access(struct gps_location *g1, struct gps_location *g2);
struct gps_location get_current_gps_location(void);

#endif
