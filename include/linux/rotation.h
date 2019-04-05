#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/rbtree.h>
#include <linux/types.h>
#include <linux/list.h>

#define WRITER 0
#define READER 1

int64_t set_rotation(int degree);
int64_t rotlock_read(int degree, int range);
int64_t rotlock_write(int degree, int range);
int64_t rotunlock_read(int degree, int range);
int64_t rotunlock_write(int degree, int range);

struct lock_node {
  pid_t pid;
  int low;
  int high;
  int grab;

  struct rb_node node;
  struct list_head lnode;
};

struct lock_tree {
  struct rb_root root;
  struct list_head head;
};

#endif
