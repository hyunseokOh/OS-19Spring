#ifndef _LINUX_GPS_H
#define _LINUX_GPS_H

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

int valid_gps_location(struct gps_location *loc);

#endif
