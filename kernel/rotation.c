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
int fromOutRange = 0;   /* whether to search available grabs from out range */
struct lock_tree writerTree = {RB_ROOT, LIST_HEAD_INIT(writerTree.head)};
struct lock_tree readerTree = {RB_ROOT, LIST_HEAD_INIT(readerTree.head)};

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
  printk("GONGLE: Node [%d], low = %d, high = %d, grab = %d\n", data->pid, data->low,
         data->high, data->grab);
}

static inline void print_tree(struct rb_root *root) {
  /* for debugging */
  struct lock_node *data = NULL;
  struct rb_node *node = NULL;
  node = root->rb_node;

  for (node = rb_first(root); node; node = rb_next(node)) {
    data = rb_entry(node, struct lock_node, node);
    print_node(data);
  }
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

static inline struct lock_node *intersect_exist(struct lock_tree *tree, int low,
                                                int high, int grabCheck) {
  struct lock_node *data = NULL;
  struct list_head *head;
  struct list_head *traverse;
  int d_low;
  int d_high;

  head = &(tree->head);

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

static inline int tree_insert(struct lock_tree *tree, struct lock_node *target) {
  struct rb_root *root = &(tree->root);
  struct rb_node **new = &(root->rb_node);
  struct rb_node *parent = NULL;
  struct lock_node *this;
  struct list_head *head = &(tree->head);
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

  /* make balance */
  rb_link_node(&(target->node), parent, new);
  rb_insert_color(&(target->node), root);

  /* add to tail */
  list_add_tail(&(target->lnode), head);

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

static inline int tree_delete(struct lock_tree *tree, struct lock_node *target) {
  struct rb_root *root = &tree->root;
  struct lock_node *data = tree_search(root, target);
  if (data != NULL) {
    /* remove from tree */
    rb_erase(&(data->node), root);

    /* remove from list */
    list_del(&(data->lnode));

    /* kfree */
    kfree(data);
    return 1;
  }
  return 0;
}

static inline int grab_locks(int type, int degree) {
  struct lock_node *data;
  struct lock_tree *tree;
  struct list_head *head;
  struct list_head *traverse;
  int totalGrab = 0;
  int d_low;
  int d_high;

  if (type == WRITER) {
    tree = &writerTree;
  } else {
    tree = &readerTree;
  }

  head = &(tree->head);

  list_for_each(traverse, head) {
    data = container_of(traverse, struct lock_node, lnode);
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

static inline int64_t grab_locks_wrapper(int type) {
  int64_t result = 0;
  if (fromOutRange) {
    if (currentDegree > 180) {
      result += (int64_t)grab_locks(type, currentDegree - 360);
    } else if (currentDegree < 180) {
      result += (int64_t)grab_locks(type, currentDegree + 360);
    }

    result += (int64_t)grab_locks(type, currentDegree);
  } else {
    result += (int64_t)grab_locks(type, currentDegree);

    if (currentDegree > 180) {
      result += (int64_t)grab_locks(type, currentDegree - 360);
    } else if (currentDegree < 180) {
      result += (int64_t)grab_locks(type, currentDegree + 360);
    }
  }
  return result;
}

int64_t set_rotation(int degree) {
  int64_t result;
  result = 0;

  /* degree update with lock */
  mutex_lock(&rot_lock);
  currentDegree = degree;

  /* First, look writer */
  result += grab_locks_wrapper(WRITER);
  if (result == 0) {
    /* no writer grabbed */
    result += grab_locks_wrapper(READER);
  }
  fromOutRange = (fromOutRange + 1) % 2;

  mutex_unlock(&rot_lock);
  /*
  if (result > 0) {
    printk("GONGLE: Current Rotation = %d, Awaked %lld nodes\n", currentDegree,
           result);
  }
  */

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
  tree_insert(&readerTree, target);
  grab_locks_wrapper(READER);
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
  tree_insert(&writerTree, target);
  grab_locks_wrapper(WRITER);
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
  tree_delete(&readerTree, &target);
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
  deleteResult = tree_delete(&writerTree, &target);
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
