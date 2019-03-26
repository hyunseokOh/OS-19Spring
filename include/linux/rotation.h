#ifndef _ROTATION_H
#define _ROTATION_H

int set_rotation(int degree);
int rotlock_read(int degree, int range);
int rotlock_write(int degree, int range);
int rotunlock_read(int degree, int range);
int rotunlock_write(int degree, int range);

#endif
