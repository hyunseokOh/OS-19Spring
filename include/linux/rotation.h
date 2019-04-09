#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/list.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/types.h>

#define WRITER 0
#define READER 1

#define RANGE_OFFSET (10)
#define ZERO_RANGE_MASK (~(511 << RANGE_OFFSET))
#define ONE_RANGE_MASK (511 << RANGE_OFFSET)

#define GET_LOW(range)                                      \
  ((*(range)&1) == 1 ? -((ZERO_RANGE_MASK & *(range)) >> 1) \
                     : (ZERO_RANGE_MASK & *(range)) >> 1)
#define GET_HIGH(range) (GET_LOW(range) + (*(range) >> RANGE_OFFSET))
#define SET_LOW(range, value)                                      \
  (*(range) = ((value > 0) ? (value << 1) : ((-value) << 1) + 1) + \
              (*(range)&ONE_RANGE_MASK))
#define SET_RANGE(range, value) \
  (*(range) = (ZERO_RANGE_MASK & *(range)) + (value << RANGE_OFFSET))
#define SET_ZERO(range) (*(range) = 0)
#define LOW(degree, range) (degree - range)
#define RANGE(range) (range << 1)

int64_t set_rotation(int degree);
int64_t rotlock_read(int degree, int range);
int64_t rotlock_write(int degree, int range);
int64_t rotunlock_read(int degree, int range);
int64_t rotunlock_write(int degree, int range);
int exit_rotlock(pid_t pid);

struct lock_node {
  pid_t pid;
  int range;
  int grab;
  struct task_struct *task;

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

static inline int is_valid(char *validRange, int low, int high) {
  int i;

  if (low >= 360) {
    low -= 360;
  }
  if (high >= 360) {
    high -= 360;
  }
  if (low < 0) {
    low += 360;
  }
  if (high < 0) {
    high += 360;
  }

  if (high < low) {
    for (i = 0; i <= high; i++) {
      if (validRange[i] == 0) {
        return 0;
      }
    }
    for (i = low; i <= 359; i++) {
      if (validRange[i] == 0) {
        return 0;
      }
    }
  } else {
    for (i = low; i <= high; i++) {
      if (validRange[i] == 0) {
        return 0;
      }
    }
  }
  return 1;
}

static inline int node_compare(struct lock_node *self,
                               struct lock_node *other) {
  /*
   * Compare between 2 lock nodes
   */
  int self_;
  int other_;

  self_ = GET_LOW(&self->range);
  other_ = GET_LOW(&other->range);
  if (self_ < other_) {
    return -1;
  } else if (self_ > other_) {
    return 1;
  } else {
    self_ = GET_HIGH(&self->range);
    other_ = GET_HIGH(&other->range);
    if (self_ < other_) {
      return -1;
    } else if (self_ > other_) {
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
         GET_LOW(&data->range), GET_HIGH(&data->range), data->grab);
}

static inline void print_list(struct list_head *head) {
  /* for debugging */
  struct lock_node *data = NULL;
  struct list_head *traverse = NULL;

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    print_node(data);
  }
  printk("\n");
}

#endif
