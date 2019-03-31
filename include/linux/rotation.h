#ifndef _LINUX_ROTATION_H
#define _LINUX_ROTATION_H

#include <linux/sched.h>
#include <linux/types.h>

#define WRITER 0
#define READER 1
#define HEIGHT(node) ((node == NULL) ? 0 : node->height)
#define MAX(node1, node2) \
  ((HEIGHT(node1) > HEIGHT(node2)) ? HEIGHT(node1) : HEIGHT(node2))
#define UPDATE_HEIGHT(node) (node->height = 1 + MAX(node->left, node->right))
#define NODE_FREE(node) ((node == NULL) ?: kfree(node))
#define RIGHT_ROTATE(node)                           \
  ({                                                 \
    struct lock_node *left__ = (node->left);         \
    struct lock_node *leftRight__ = (left__->right); \
    left__->right = node;                            \
    node->left = leftRight__;                        \
    UPDATE_HEIGHT(node);                             \
    UPDATE_HEIGHT(left__);                           \
    left__;                                          \
  })
#define LEFT_ROTATE(node)                            \
  ({                                                 \
    struct lock_node *right__ = (node->right);       \
    struct lock_node *rightLeft__ = (right__->left); \
    right__->left = node;                            \
    node->right = rightLeft__;                       \
    UPDATE_HEIGHT(node);                             \
    UPDATE_HEIGHT(right__);                          \
    right__;                                         \
  })
#define DOUBLE_RIGHT_ROTATE(node)          \
  ({                                       \
    struct lock_node *left__ = node->left; \
    node->left = LEFT_ROTATE(left__);      \
    RIGHT_ROTATE(node);                    \
  })
#define DOUBLE_LEFT_ROTATE(node)             \
  ({                                         \
    struct lock_node *right__ = node->right; \
    node->right = RIGHT_ROTATE(right__);     \
    LEFT_ROTATE(node);                       \
  })
#define LEFTMOST_CHILD(node)     \
  ({                             \
    while (node->left != NULL) { \
      node = node->left;         \
    }                            \
    node;                        \
  })
#define INTERSECT(r_low, low, r_high, high) ((r_high > low) && (r_low < high))

int64_t set_rotation(int degree);
int64_t rotlock_read(int degree, int range);
int64_t rotlock_write(int degree, int range);
int64_t rotunlock_read(int degree, int range);
int64_t rotunlock_write(int degree, int range);

struct lock_node {
  pid_t pid;
  int range[2];
  int height;
  int grab;

  struct lock_node *left;
  struct lock_node *right;
  struct task_struct *task;
};

struct lock_tree {
  struct lock_node *root;
};

struct lock_node *tree_insert_(struct lock_node *root,
                               struct lock_node *target);
struct lock_node *tree_delete_(struct lock_node *root,
                               struct lock_node *target);
int tree_find_(struct lock_node *root, int low, int high);
void tree_awake_(struct lock_node *root, int degree, int type,
                              int *totalAwaken);

#endif
