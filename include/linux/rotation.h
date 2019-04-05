#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/list.h>
#include <linux/printk.h>
#include <linux/types.h>

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

  struct list_head lnode;
};

/* some short helpers */
static inline int in_range(int degree, int low, int high) {
  int subDegree;
  if (degree < 180) {
    /* check for degree + 360 also */
    subDegree = degree + 360;
    return (low <= degree && degree <= high) ||
           (low <= subDegree && subDegree <= high);
  } else if (degree > 180) {
    /* check for degree - 360 also */
    subDegree = degree - 360;
    return (low <= degree && degree <= high) ||
           (low <= subDegree && subDegree <= high);
  } else {
    /* exact 180 */
    return low <= degree && degree <= high;
  }
}

static inline int node_compare(struct lock_node *self,
                               struct lock_node *other) {
  /*
   * Compare between 2 lock nodes
   */
  if (self->low < other->low) {
    return -1;
  } else if (self->low > other->low) {
    return 1;
  } else {
    if (self->high < other->high) {
      return -1;
    } else if (self->high > other->high) {
      return 1;
    } else {
      return self->pid - other->pid;
    }
  }
}

static inline void print_node(struct lock_node *data) {
  /* for debugging */
  if (data == NULL) {
    return;
  }
  printk("GONGLE: Node [%d], low = %d, high = %d, grab = %d\n", data->pid,
         data->low, data->high, data->grab);
}

static inline void print_list(struct list_head *head) {
  /* for debugging */
  struct lock_node *data = NULL;
  struct list_head *traverse = NULL;

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    print_node(data);
  }
}

#endif
