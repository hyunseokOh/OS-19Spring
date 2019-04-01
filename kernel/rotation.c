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

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(rotlock_mutex);  /* for access shared data */
static int currentDegree = 0; /* current device rotation */

static struct rb_root writerTree = RB_ROOT;
static struct rb_root readerTree = RB_ROOT;

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

  return node;
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

static inline struct lock_node *intersect_exist(struct rb_root *root, int low,
                                                int high, int grabCheck) {
  struct lock_node *data = NULL;
  struct rb_node *node = NULL;
  int d_low;
  int d_high;

  node = root->rb_node;

  for (node = rb_first(root); node; node = rb_next(node)) {
    /* check inorder */
    data = rb_entry(node, struct lock_node, node);
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

static inline int tree_insert(struct rb_root *root, struct lock_node *target) {
  struct rb_node **new = &(root->rb_node);
  struct rb_node *parent = NULL;
  struct lock_node *this;
  int compare;

  while (*new) {
    this = container_of(*new, struct lock_node, node);
    compare = node_compare(target, this);

    parent = *new;
    if (compare < 0) {
      /* target is smaller */
      new = &((*new)->rb_left);
    } else if (compare > 0) {
      new = &((*new)->rb_right);
    } else {
      /* insert failed */
      return 0;
    }
  }
  rb_link_node(&target->node, parent, new);
  rb_insert_color(&target->node, root);
  return 1;
}

static inline struct lock_node *tree_search(struct rb_root *root,
                                            struct lock_node *target) {
  /* search for exact match */
  struct rb_node *node;
  struct lock_node *data;
  int compare;

  node = root->rb_node;

  while (node) {
    data = container_of(node, struct lock_node, node);
    compare = node_compare(target, data);
    if (compare < 0) {
      /* target < data */
      node = node->rb_left;
    } else if (compare > 0) {
      /* target > data */
      node = node->rb_right;
    } else {
      return data;
    }
  }
  return NULL;
}

static inline int tree_delete(struct rb_root *root, struct lock_node *target) {
  struct lock_node *data = tree_search(root, target);
  if (data != NULL) {
    rb_erase(&(data->node), root);
    kfree(data);
    return 1;
  }
  return 0;
}

static inline int grab_locks(int type) {
  struct rb_node *node;
  struct rb_root *root;
  struct lock_node *data;
  int totalGrab = 0;
  int d_low;
  int d_high;

  if (type == WRITER) {
    root = &writerTree;
  } else {
    root = &readerTree;
  }

  node = root->rb_node;

  for (node = rb_first(root); node; node = rb_next(node)) {
    /* check inorder */
    data = rb_entry(node, struct lock_node, node);
    d_low = data->low;
    d_high = data->high;
    if (d_low <= currentDegree && currentDegree <= d_high) {
      /* possible candidate */
      if (!data->grab) {
        if (type == WRITER) {
          /*
           * Writer can grab lock iff
           *  1. No writer grabs lock intersected range currently
           *  2. No reader grabs lock intersected range currently
           */
          if (intersect_exist(&writerTree, d_low, d_high, 1) == NULL &&
              intersect_exist(&readerTree, d_low, d_high, 1) == NULL) {
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
          if (intersect_exist(&writerTree, d_low, d_high, 0) == NULL) {
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

  /* degree update with lock */
  mutex_lock(&rotlock_mutex);
  currentDegree = degree;

  /* First, look writer */
  result += (int64_t)grab_locks(WRITER);

  if (result == 0) {
    /* Should return 0 if writer already grabbed lock */
    result += (int64_t)grab_locks(READER);
  }

  printk("GONGLE: Current Rotation = %d, Awaked %lld nodes\n", currentDegree,
         result);

  mutex_unlock(&rotlock_mutex);
  return result;
}

int64_t rotlock_read(int degree, int range) {
  struct lock_node *target = NULL;
  int r_low;
  int r_high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  r_low = degree - range;
  r_high = degree + range;

  /* lock before access shared data structure */
  target = node_init(r_low, r_high);
  if (target == NULL) {
    return -ENOMEM;
  }

  /* shared data access */
  mutex_lock(&rotlock_mutex);
  tree_insert(&readerTree, target);
  grab_locks(READER);
  mutex_unlock(&rotlock_mutex);

  return 0;
}

int64_t rotlock_write(int degree, int range) {
  struct lock_node *target = NULL;
  int r_low;
  int r_high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  r_low = degree - range;
  r_high = degree + range;

  printk("GONGLE: request write lock for range %d to %d\n", r_low, r_high);
  target = node_init(r_low, r_high);

  if (target == NULL) {
    return -ENOMEM;
  }

  mutex_lock(&rotlock_mutex);
  tree_insert(&writerTree, target);
  grab_locks(WRITER);
  mutex_unlock(&rotlock_mutex);
  return 0;
}

int64_t rotunlock_read(int degree, int range) {
  struct lock_node target;
  int r_low;
  int r_high;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  r_low = degree - range;
  r_high = degree + range;

  target.low = r_low;
  target.high = r_high;
  target.pid = current->pid;

  mutex_lock(&rotlock_mutex);
  tree_delete(&readerTree, &target);
  mutex_unlock(&rotlock_mutex);

  return 0;
}

int64_t rotunlock_write(int degree, int range) {
  struct lock_node target;
  int r_low;
  int r_high;
  int deleteResult;

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }
  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  r_low = degree - range;
  r_high = degree + range;

  target.low = r_low;
  target.high = r_high;
  target.pid = current->pid;

  mutex_lock(&rotlock_mutex);
  deleteResult = tree_delete(&writerTree, &target);
  if (deleteResult) {
    printk("GONGLE: unlock successed\n");
  } else {
    printk("GONGLE: unlock failed :(\n");
  }
  mutex_unlock(&rotlock_mutex);

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
