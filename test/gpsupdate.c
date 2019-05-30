#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define SYS_SET_GPS_LOCATION 398

struct gps_location {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
};

int convert_arg(char *args[6], int idx) {
  long long_arg;
  char *ptr = NULL;

  long_arg = strtol(args[idx], &ptr, 10);

  if (*ptr) {
    printf("Failed to convert command line argument %s to int\n", args[idx]);
    exit(0);
  }

  return (int) long_arg;

}

int main(int argc, char *argv[]) {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
  int syscallResult;

  if (argc != 6) {
    printf("Must pass required arguments!\n");
    return 0;
  }

  lat_integer = convert_arg(argv, 1);
  lat_fractional = convert_arg(argv, 2);
  lng_integer = convert_arg(argv, 3);
  lng_fractional = convert_arg(argv, 4);
  accuracy = convert_arg(argv, 5);

  struct gps_location gps = {
    lat_integer,
    lat_fractional,
    lng_integer,
    lng_fractional,
    accuracy
  };

  syscallResult = syscall(SYS_SET_GPS_LOCATION, &gps);

  if (syscallResult == -1) {
    printf("Oops! syscall returned -1, something wrong\n");
    return 0;
  }
}
