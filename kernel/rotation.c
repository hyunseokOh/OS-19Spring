#include <linux/linkage.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/rotation.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <uapi/asm-generic/errno-base.h>

DEFINE_MUTEX(rotlock_mutex);  /* for access shared data */
static int currentDegree = 0; /* current device rotation */

/* tree for write lock request */
static struct lock_tree writerRequested = {.root = NULL};
/* tree for reader lock request */
static struct lock_tree readerRequested = {.root = NULL};
/* tree for locked writer */
static struct lock_tree writerLocked = {.root = NULL};
/* tree for locked reader */
static struct lock_tree readerLocked = {.root = NULL};

/*
 * Lock Node Related functions
 */
static inline struct lock_node *node_init(int r_low, int r_high) {
  /*
   * Constructor for lock_node
   */
  struct lock_node *node = NULL;
  struct task_struct *task = NULL;
  node = (struct lock_node *)kmalloc(sizeof(struct lock_node), GFP_KERNEL);

  if (node == NULL) {
    /* kmalloc failed */
    return NULL;
  }

  task = current;

  node->pid = current->pid;
  node->range[0] = r_low;
  node->range[1] = r_high;
  node->height = 1;
  node->grab = 0;
  node->left = NULL;
  node->right = NULL;
  node->task = task;

  return node;
}

static inline int node_compare(struct lock_node *self,
                               struct lock_node *other) {
  /*
   * Compare between 2 lock nodes
   */
  if (self->range[0] < other->range[0]) {
    return -1;
  } else if (self->range[0] > other->range[0]) {
    return 1;
  } else {
    if (self->range[1] < other->range[1]) {
      return -1;
    } else if (self->range[1] > other->range[1]) {
      return 1;
    } else {
      if (self->pid < other->pid) {
        return -1;
      } else if (self->pid > other->pid) {
        return 1;
      } else {
        return 0;
      }
    }
  }
}

static inline struct lock_node *node_balance(struct lock_node *root) {
  int leftHeight;
  int rightHeight;

  leftHeight = HEIGHT(root->left);
  rightHeight = HEIGHT(root->right);

  if (leftHeight - rightHeight == 2) {
    if (HEIGHT(root->left->right) > HEIGHT(root->left->left)) {
      root = DOUBLE_RIGHT_ROTATE(root);
    } else {
      root = RIGHT_ROTATE(root);
    }
  }

  if (rightHeight - leftHeight == 2) {
    if (HEIGHT(root->right->left) > HEIGHT(root->right->right)) {
      root = DOUBLE_LEFT_ROTATE(root);
    } else {
      root = LEFT_ROTATE(root);
    }
  }

  return root;
}

static inline void tree_free(struct lock_tree *self) {
  /*
   * free given tree pointer
   * (maybe not be needed)
   */
  struct lock_node *node = NULL;
  int deleteSuccess = 0;

  if (self == NULL) {
    return;
  }

  node = self->root;
  while (node != NULL) {
    self->root = tree_delete_(self->root, node, &deleteSuccess);
    kfree(node);
    node = self->root;
  }
  kfree(self);
}

static inline int tree_insert(struct lock_tree *self, int degree, int range) {
  struct lock_node *target = NULL;
  int r_low;
  int r_high;

  if (self == NULL) {
    return -EINVAL;
  }

  if (degree < 0 || degree >= 360) {
    return -EINVAL;
  }

  if (range <= 0 || range >= 180) {
    return -EINVAL;
  }

  r_low = degree - range;
  r_high = degree + range;
  target = node_init(r_low, r_high);
  self->root = tree_insert_(self->root, target);
  return 0;
}

static inline int tree_delete(struct lock_node *target, int type) {
  struct lock_tree *self = NULL;
  int deleteSuccess = 0;

  if (type == WRITER) {
    self = &writerLocked;
  } else {
    self = &readerLocked;
  }

  self->root = tree_delete_(self->root, target, &deleteSuccess);

  return deleteSuccess;
}

static inline int tree_awake(int type) {
  /*
   * Should be called inside of set_rotation
   * Wrapper function of recursive function tree_awake_,
   * which traverse requestedTree and if currentDegree is inside of node range,
   * remove that node from requestedTree, and insert into lockedTree with
   * node->grab = 1
   * This function returns whole number of awakened node
   *
   * Support polymorphism for both readerTree and writerTree
   */
  struct lock_tree *self;
  int totalAwaken;

  if (type == WRITER) {
    self = &writerRequested;
  } else {
    self = &readerRequested;
  }

  totalAwaken = 0;

  if (self == NULL) {
    return 0;
  }
  if (currentDegree < -179 || currentDegree > 538) {
    return -1;
  }

  if (currentDegree < 180) {
    tree_awake_(self->root, currentDegree, type, &totalAwaken);
    tree_awake_(self->root, currentDegree + 360, type, &totalAwaken);
  } else if (currentDegree > 180) {
    tree_awake_(self->root, currentDegree, type, &totalAwaken);
    tree_awake_(self->root, currentDegree - 360, type, &totalAwaken);
  } else {
    /*
     * CurrentDegree == 180
     */
    tree_awake_(self->root, currentDegree, type, &totalAwaken);
  }

  return totalAwaken;
}

struct lock_node *tree_insert_(struct lock_node *root,
                               struct lock_node *target) {
  if (root == NULL) {
    return target;
  }

  if (node_compare(root, target) < 0) {
    root->right = tree_insert_(root->right, target);
  } else {
    root->left = tree_insert_(root->left, target);
  }

  UPDATE_HEIGHT(root);
  root = node_balance(root);
  return root;
}

struct lock_node *tree_delete_(struct lock_node *root,
                               struct lock_node *target,
                               int *deleteSuccess) {
  struct lock_node *leftmost;
  int compare;

  if (root == NULL) {
    return NULL;
  }
  if (target == NULL) {
    return root;
  }

  compare = node_compare(root, target);

  if (compare == 1) {
    /* root is bigger */
    root->left = tree_delete_(root->left, target, deleteSuccess);
  } else if (compare == -1) {
    root->right = tree_delete_(root->right, target, deleteSuccess);
  } else {
    *deleteSuccess = 1;
    if (root->left == NULL && root->right == NULL) {
      return NULL;
    } else if (root->left == NULL && root->right != NULL) {
      root = root->right;
    } else if (root->left != NULL && root->right == NULL) {
      root = root->left;
    } else {
      leftmost = LEFTMOST_CHILD(root->right);
      root->right = tree_delete_(root->right, leftmost, deleteSuccess);
      leftmost->right = root->right;
      leftmost->left = root->left;
      root = leftmost;
    }
  }
  UPDATE_HEIGHT(root);
  root = node_balance(root);
  return root;
}

int tree_find_(struct lock_node *root, int low, int high) {
  /*
   * Check whether intersection exists
   */
  int r_low;
  int r_high;

  if (root == NULL) {
    return 0;
  }

  r_low = root->range[0];
  r_high = root->range[1];

  if (INTERSECT(r_low, low, r_high, high)) {
    /*
     * TODO (taebum): Does intersection includes same boundary?
     * is this condition fully cover all intersections?
     */
    return 1;
  } else {
    if (r_high <= low) {
      return tree_find_(root->right, low, high);
    } else {
      /* r_low >= high */
      return tree_find_(root->left, low, high);
    }
  }
}

void tree_awake_(struct lock_node *root, int degree, int type,
                              int *totalAwaken) {
  int r_low;
  int r_high;
  struct lock_node *removed;
  int success;
  if (root == NULL) {
    return;
  }

  r_low = root->range[0];
  r_high = root->range[1];

  if (r_low <= degree && degree <= r_high) {
    removed = root;
    if (type == WRITER) {
      /*
       * If this writer wants to grab a lock,
       *  1. No current writer lock exist
       *  2. No current reader lock exist
       */
      if (!tree_find_(writerLocked.root, r_low, r_high) && !tree_find_(readerLocked.root, r_low, r_high)) {
        writerRequested.root = tree_delete_(writerRequested.root, removed, &success);
        *totalAwaken = *totalAwaken + 1;
        removed->left = NULL;
        removed->right = NULL;
        writerLocked.root = tree_insert_(writerLocked.root, removed);
        removed->grab = 1;
      }
    } else {
      /*
       * If this reader wants to grab a lock
       *  1. No current writer lock exist
       *  2. No writer is waiting for this range
       */
      if (!tree_find_(writerLocked.root, r_low, r_high) && !tree_find_(writerRequested.root, r_low, r_high)) {
        readerRequested.root = tree_delete_(readerRequested.root, removed, &success);
        *totalAwaken = *totalAwaken + 1;
        removed->left = NULL;
        removed->right = NULL;
        readerLocked.root = tree_insert_(readerLocked.root, removed);
        removed->grab = 1;

        /* start from root again since multiple reader locks allowed */
        tree_awake_(readerRequested.root, degree, type, totalAwaken);
      }
    }
  } else {
    tree_awake_(root->left, degree, type, totalAwaken);
    tree_awake_(root->right, degree, type, totalAwaken);
  }
}

int64_t set_rotation(int degree) {
  int64_t result;
  result = 0;

  /* degree update with lock */
  mutex_lock(&rotlock_mutex);
  currentDegree = degree;

  /* First, look writer */
  result += (int64_t)tree_awake(WRITER);

  /* Should return 0 if writer already grabbed lock */
  result += (int64_t)tree_awake(READER);

  printk("GONGLE: Current Rotation = %d, Awaked %lld nodes\n", currentDegree, result);

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
  mutex_lock(&rotlock_mutex);
  target = node_init(r_low, r_high);
  if (r_low <= currentDegree && currentDegree <= r_high) {
    if (!tree_find_(writerLocked.root, r_low, r_high)) {
      /* grab lock directly */
      target->grab = 1;
      readerLocked.root = tree_insert_(readerLocked.root, target);
      mutex_unlock(&rotlock_mutex);
      return 0;
    }
  }
  /*
   * write lock exists or currentDegree is not in range
   * insert to requestTree and wait
   */
  readerRequested.root = tree_insert_(readerRequested.root, target);
  mutex_unlock(&rotlock_mutex);
  /*
   * TODO (taebum)
   * Block process
   */

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
  if (r_low <= currentDegree && currentDegree <= r_high) {
    if (!tree_find_(writerLocked.root, r_low, r_high) && !tree_find_(readerLocked.root, r_low, r_high)) {
      /* grab lock directly */
      printk("GONGLE: grab directly\n");
      target->grab = 1;
      writerLocked.root = tree_insert_(writerLocked.root, target);
      mutex_unlock(&rotlock_mutex);
      return 0;
    }
  }

  printk("GONGLE: add to request\n");
  writerRequested.root = tree_insert_(writerRequested.root, target);
  mutex_unlock(&rotlock_mutex);
  /*
   * TODO (taebum)
   * Block process
   */
  return 0;
}

int64_t rotunlock_read(int degree, int range) { 
  struct lock_node target ;
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

  target.range[0] = r_low;
  target.range[1] = r_high;
  target.pid = current->pid;

  mutex_lock(&rotlock_mutex);
  tree_delete(&target, READER);
  mutex_unlock(&rotlock_mutex);

  return 0; 
}

int64_t rotunlock_write(int degree, int range) { 
  struct lock_node target ;
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

  target.range[0] = r_low;
  target.range[1] = r_high;
  target.pid = current->pid;

  mutex_lock(&rotlock_mutex);
  deleteResult = tree_delete(&target, WRITER);
  if (deleteResult) {
    printk("GOGLE: unlock successed\n");
  } else {
    printk("GOGLE: unlock failed :(\n");
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
