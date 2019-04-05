#include <linux/linkage.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/rbtree.h>
#include <linux/rotation.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/list.h>

#include <uapi/asm-generic/errno-base.h>

DECLARE_WAIT_QUEUE_HEAD(wait_head); /* wait queue */
DEFINE_MUTEX(rot_lock); /* for access shared data */
int currentDegree = 0;  /* current device rotation */
struct list_head writerList = LIST_HEAD_INIT(writerList);
struct list_head readerList = LIST_HEAD_INIT(readerList);


static inline int is_grab(struct lock_node *data) {
  /*
   * Safe checking for whether grabbed
   */
  int result = 0;
  mutex_lock(&rot_lock);
  result = data->grab;
  mutex_unlock(&rot_lock);
  return result;
}

static inline struct lock_node *node_init(int r_low, int r_high) {
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
  node->low = r_low;
  node->high = r_high;
  node->grab = 0;
  INIT_LIST_HEAD(&node->lnode);

  return node;
}

static inline struct lock_node *is_intersect(struct list_head *from, struct list_head *head,
                                            int low, int high, int grabCheck) {
  /* check intersection from starting point (not used currently) */
  struct lock_node *data;
  int d_low;
  int d_high;
  
  for (from = (from)->next; from != (head); from = from->next) {
    data = container_of(from, struct lock_node, lnode);
    d_low = data->low;
    d_high = data->high;
    if (low <= d_high && high >= d_low) {
      /* inter section found */
      if (grabCheck) {
        if (data->grab) {
          return data;
        }
      } else {
        return data;
      }
    }
  }
  return NULL;

}

static inline struct lock_node *intersect_exist(struct list_head *head, int low,
                                                int high, int grabCheck) {
  struct lock_node *data = NULL;
  struct list_head *traverse;
  int d_low;
  int d_high;

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    d_low = data->low;
    d_high = data->high;
    if (low <= d_high && high >= d_low) {
      /* inter section found */
      if (grabCheck) {
        if (data->grab) {
          return data;
        }
      } else {
        return data;
      }
    }
  }
  return NULL;
}

static inline int list_delete(struct list_head *head, struct lock_node *target) {
  struct lock_node *data;
  struct list_head *traverse;
  int compare;
  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    compare = node_compare(data, target);
    if (compare == 0) {
      /* match */
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

  if (type == WRITER) {
    head = &writerList;
  } else {
    head = &readerList;
  }

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
    d_low = data->low;
    d_high = data->high;
    if (in_range(currentDegree, d_low, d_high)) {
      /* possible candidate */
      if (!data->grab) {
        if (type == WRITER) {
          /*
           * Writer can grab lock iff
           *  1. No writer grabs lock intersected range currently
           *  2. No reader grabs lock intersected range currently
           */
          if (intersect_exist(&writerList, d_low, d_high, 1) == NULL &&
              intersect_exist(&readerList, d_low, d_high, 1) == NULL) {
            /* can grab! */
            data->grab = 1;
            totalGrab++;

            /* no more writer available in currentDegree*/
            break;
          }
        } else {
          /*
           * Reader can grab lock iff
           *  1. No writer grabs lock intersected range currently
           *  2. No writer waits lock intersected range currently
           */
          if (intersect_exist(&writerList, d_low, d_high, 0) == NULL) {
            /* can grab! */
            data->grab = 1;
            totalGrab++;
          }
        }
      }
    }
  }
  return totalGrab;
}

int64_t set_rotation(int degree) {
  int64_t result;
  result = 0;

  mutex_lock(&rot_lock);

  /* degree update with lock */
  currentDegree = degree;

  /* First, look writer */
  result += grab_locks(WRITER);
  
  /* Next, look reader */
  result += grab_locks(READER);

  mutex_unlock(&rot_lock);

  wake_up(&wait_head); /* wake up others */
  return result;
}

int64_t rotlock_read(int degree, int range) {
  struct lock_node *target = NULL;
  int low;
  int high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  low = degree - range;
  high = degree + range;

  /* lock before access shared data structure */
  target = node_init(low, high);
  if (target == NULL) {
    return -ENOMEM;
  }

  /* shared data access */
  mutex_lock(&rot_lock);
  list_add_tail(&target->lnode, &readerList);
  if (in_range(currentDegree, low, high)) {
    grab_locks(READER);
  }
  mutex_unlock(&rot_lock);

  DEFINE_WAIT(wait);
  add_wait_queue(&wait_head, &wait);
  while (!is_grab(target)) {
    prepare_to_wait(&wait_head, &wait, TASK_INTERRUPTIBLE);
    if (signal_pending(current)) {
      break;
    }
    schedule();
  }
  finish_wait(&wait_head, &wait);

  return 0;
}

int64_t rotlock_write(int degree, int range) {
  struct lock_node *target = NULL;
  int low;
  int high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  low = degree - range;
  high = degree + range;

  target = node_init(low, high);

  if (target == NULL) {
    return -ENOMEM;
  }

  mutex_lock(&rot_lock);
  list_add_tail(&target->lnode, &writerList);
  if (in_range(currentDegree, low, high)) {
    grab_locks(WRITER);
  }
  mutex_unlock(&rot_lock);

  DEFINE_WAIT(wait);
  add_wait_queue(&wait_head, &wait);
  while (!is_grab(target)) {
    prepare_to_wait(&wait_head, &wait, TASK_INTERRUPTIBLE);
    if (signal_pending(current)) {
      break;
    }
    schedule();
  }
  finish_wait(&wait_head, &wait);
  return 0;
}

int64_t rotunlock_read(int degree, int range) {
  struct lock_node target;
  int low;
  int high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  low = degree - range;
  high = degree + range;

  target.low = low;
  target.high = high;
  target.pid = current->pid;

  mutex_lock(&rot_lock);
  list_delete(&readerList, &target);
  mutex_unlock(&rot_lock);
  wake_up(&wait_head);

  return 0;
}

int64_t rotunlock_write(int degree, int range) {
  struct lock_node target;
  int low;
  int high;
  int deleteResult;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  low = degree - range;
  high = degree + range;

  target.low = low;
  target.high = high;
  target.pid = current->pid;

  mutex_lock(&rot_lock);
  deleteResult = list_delete(&writerList, &target);
  mutex_unlock(&rot_lock);
  wake_up(&wait_head);

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
