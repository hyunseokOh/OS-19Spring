#include <linux/linkage.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/rotation.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/types.h>

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(rot_lock); /* for access shared data */
int currentDegree = 0;  /* current device rotation */
struct list_head writerList = LIST_HEAD_INIT(writerList);
struct list_head readerList = LIST_HEAD_INIT(readerList);

static inline bool is_grab(struct lock_node *data) {
  /*
   * Safe checking for whether grabbed
   */
  bool result = 0;
  mutex_lock(&rot_lock);
  result = data->grab;
  mutex_unlock(&rot_lock);
  return result;
}

int exit_rotlock(pid_t pid) {
  /* iterate over list, clear up all requests, lock, called by pid */
  struct lock_node *data;
  struct list_head *traverse;
  struct list_head *tmp;
  struct list_head *head;
  int totalDelete = 0;

  mutex_lock(&rot_lock);
  head = &readerList;
  list_for_each_safe(traverse, tmp, head) {
    data = container_of(traverse, struct lock_node, lnode);
    if (data->pid == pid) {
      list_del(&data->lnode);
      kfree(data);
      totalDelete++;
    }
  }

  head = &writerList;
  list_for_each_safe(traverse, tmp, head) {
    data = container_of(traverse, struct lock_node, lnode);
    if (data->pid == pid) {
      list_del(&data->lnode);
      kfree(data);
      totalDelete++;
    }
  }

  /* try to grab other available locks */
  grab_locks(WRITER);
  grab_locks(READER);

  mutex_unlock(&rot_lock);

  return totalDelete;
}

static inline struct lock_node *node_init(int degree, int range) {
  /*
   * Constructor for lock_node
   */
  struct lock_node *node = NULL;
  node = (struct lock_node *)kmalloc(sizeof(struct lock_node), GFP_KERNEL);
  if (node == NULL) {
    /* kmalloc failed */
    return NULL;
  }
  node->pid = current->pid;
  SET_ZERO(&node->range);
  SET_LOW(&node->range, LOW(degree, range));
  SET_RANGE(&node->range, RANGE(range));
  node->grab = false;
  node->task = current;
  INIT_LIST_HEAD(&node->lnode);

  return node;
}

static inline void mask_invalid(struct list_head *head, char *validRange,
                                int grabCheck) {
  struct list_head *traverse = NULL;
  struct lock_node *data = NULL;
  int d_low;
  int d_high;
  int i;

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    d_low = GET_LOW(&data->range);
    d_high = GET_HIGH(&data->range);
    if (d_low < 0) {
      d_low += 360;
    }
    if (d_high < 0) {
      d_high += 360;
    }
    if (d_low >= 360) {
      d_low -= 360;
    }
    if (d_high >= 360) {
      d_high -= 360;
    }
    if (grabCheck) {
      if (data->grab) {
        /* make invalid */
        if (d_high < d_low) {
          for (i = 0; i <= d_high; i++) {
            validRange[i] = 0;
          }
          for (i = d_low; i <= 359; i++) {
            validRange[i] = 0;
          }
        } else {
          /* d_high > d_low */
          for (i = d_low; i <= d_high; i++) {
            validRange[i] = 0;
          }
        }
      }
    } else {
      /* make invalid directly */
      if (d_high < d_low) {
        for (i = 0; i <= d_high; i++) {
          validRange[i] = 0;
        }
        for (i = d_low; i <= 359; i++) {
          validRange[i] = 0;
        }
      } else {
        /* d_high > d_low */
        for (i = d_low; i <= d_high; i++) {
          validRange[i] = 0;
        }
      }
    }
  }
}

static inline int list_delete(struct list_head *head,
                              struct lock_node *target) {
  struct lock_node *data;
  struct list_head *traverse;
  struct list_head *tmp;
  int compare;
  list_for_each_safe(traverse, tmp, head) {
    data = container_of(traverse, struct lock_node, lnode);
    compare = node_compare(data, target);
    if (compare == 0 && data->grab) {
      /*
       * match! delete
       */
      list_del(&data->lnode);
      kfree(data);
      return 1;
    }
  }
  return 0;
}

static inline int grab_locks(int type) {
  struct lock_node *data;
  struct list_head *head;
  struct list_head *traverse;
  int totalGrab = 0;
  int d_low;
  int d_high;
  char validRange[360];
  int i;

  for (i = 0; i < 360; i++) {
    /* initialize */
    validRange[i] = 1;
  }

  if (type == WRITER) {
    head = &writerList;
    mask_invalid(&writerList, validRange, 1);
    mask_invalid(&readerList, validRange, 1);
  } else {
    head = &readerList;
    mask_invalid(&writerList, validRange, 0);
  }

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    d_low = GET_LOW(&data->range);
    d_high = GET_HIGH(&data->range);
    if (!data->grab && in_range(currentDegree, d_low, d_high)) {
      if (is_valid(validRange, d_low, d_high)) {
        data->grab = true;
        totalGrab++;
        wake_up_process(data->task);
        if (type == WRITER) {
          /* no more writer is available in current degree */
          break;
        }
      }
    }
  }
  return totalGrab;
}

int64_t set_rotation(int degree) {
  int64_t result;
  result = 0;

  if (degree < 0 || degree >= 360) {
    /* error case */
    return -EINVAL;
  }

  mutex_lock(&rot_lock);

  /* degree update with lock */
  currentDegree = degree;

  /* First, look writer */
  result += grab_locks(WRITER);

  /* Next, look reader */
  result += grab_locks(READER);

  mutex_unlock(&rot_lock);

  return result;
}

int64_t rotlock_read(int degree, int range) {
  struct lock_node *target = NULL;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  target = node_init(degree, range);

  if (target == NULL) {
    return -ENOMEM;
  }

  /* shared data access */
  mutex_lock(&rot_lock);
  list_add_tail(&target->lnode, &readerList);
  if (in_range(currentDegree, GET_LOW(&target->range),
               GET_HIGH(&target->range))) {
    /*
     * Try to grab lock directly, however, it could be failed if
     * there is writer lock (grab or wait either)
     */
    grab_locks(READER);
  }
  mutex_unlock(&rot_lock);

  set_current_state(TASK_INTERRUPTIBLE);
  mutex_lock(&rot_lock);
  while (!target->grab) {
    mutex_unlock(&rot_lock);
    schedule();
    if (signal_pending(current)) {
      list_del(&target->lnode);
      kfree(target);
      return -EINTR;
    } else {
      set_current_state(TASK_INTERRUPTIBLE);
      mutex_lock(&rot_lock);
    }
  }
  __set_current_state(TASK_RUNNING);
  mutex_unlock(&rot_lock);
  return 0;
}

int64_t rotlock_write(int degree, int range) {
  struct lock_node *target = NULL;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  target = node_init(degree, range);

  if (target == NULL) {
    /* allocation failed */
    return -ENOMEM;
  }

  mutex_lock(&rot_lock);
  list_add_tail(&target->lnode, &writerList);
  if (in_range(currentDegree, GET_LOW(&target->range),
               GET_HIGH(&target->range))) {
    /*
     * try to grab lock directly
     */
    grab_locks(WRITER);
  }
  mutex_unlock(&rot_lock);

  set_current_state(TASK_INTERRUPTIBLE);
  mutex_lock(&rot_lock);
  while (!target->grab) {
    mutex_unlock(&rot_lock);
    schedule();
    if (signal_pending(current)) {
      list_del(&target->lnode);
      kfree(target);
      return -EINTR;
    } else {
      set_current_state(TASK_INTERRUPTIBLE);
      mutex_lock(&rot_lock);
    }
  }
  __set_current_state(TASK_RUNNING);
  mutex_unlock(&rot_lock);
  return 0;
}

int64_t rotunlock_read(int degree, int range) {
  struct lock_node target;
  int deleteResult;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  target.range = 0;
  SET_LOW(&(target.range), LOW(degree, range));
  SET_RANGE(&(target.range), RANGE(range));
  target.pid = current->pid;

  mutex_lock(&rot_lock);
  deleteResult = list_delete(&readerList, &target);
  if (deleteResult) {
    /* delete success, try to grab others */
    grab_locks(WRITER);
    grab_locks(READER);
  }
  mutex_unlock(&rot_lock);
  return 0;
}

int64_t rotunlock_write(int degree, int range) {
  struct lock_node target;
  int deleteResult;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  target.range = 0;
  SET_LOW(&(target.range), LOW(degree, range));
  SET_RANGE(&(target.range), RANGE(range));
  target.pid = current->pid;

  mutex_lock(&rot_lock);
  deleteResult = list_delete(&writerList, &target);
  if (deleteResult) {
    /* delete success, try to grab others */
    grab_locks(WRITER);
    grab_locks(READER);
  }
  mutex_unlock(&rot_lock);
  return 0;
}

asmlinkage long sys_set_rotation(int degree) { return set_rotation(degree); }

asmlinkage long sys_rotlock_read(int degree, int range) {
  return rotlock_read(degree, range);
}

asmlinkage long sys_rotlock_write(int degree, int range) {
  return rotlock_write(degree, range);
}

asmlinkage long sys_rotunlock_read(int degree, int range) {
  return rotunlock_read(degree, range);
}

asmlinkage long sys_rotunlock_write(int degree, int range) {
  return rotunlock_write(degree, range);
}
