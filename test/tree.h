#ifndef _TREE_H
#define _TREE_H

#define WRITER 0
#define READER 1
#define MAX_HEIGHT(node1, node2) \
  ((height(node1) > height(node2)) ? height(node1) : height(node2))
#define height(node) ((node == NULL) ? 0 : node->height)

#ifdef DEBUG
#undef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#include <sys/types.h>

struct lock_node {
  pid_t pid;
  int range[2];
  int node_type;
  int height;

  struct lock_node *left;
  struct lock_node *right;
  struct lock_node *next;
};

struct lock_node *node_init(pid_t pid, int r_low, int r_high, int node_type);
void node_delete(struct lock_node *node);
void print_node(struct lock_node *node);
int node_compare(struct lock_node *self, struct lock_node *other);
struct lock_node *right_rotate(struct lock_node *node);
struct lock_node *left_rotate(struct lock_node *node);
struct lock_node *double_right_rotate(struct lock_node *node);
struct lock_node *double_left_rotate(struct lock_node *node);
struct lock_node *node_balance(struct lock_node *node);

struct lock_tree {
  /* Tree structure of lock request */
  struct lock_node *root;
};

struct lock_tree *tree_init(void);
void tree_delete(struct lock_tree *self);
int tree_insert(struct lock_tree *self, pid_t pid, int degree, int range,
                int node_type);
struct lock_node *tree_insert_(struct lock_node *root,
                               struct lock_node *target);

struct lock_list *tree_find(struct lock_tree *self, int degree);
void tree_find_(struct lock_node *root, int degree, struct lock_list **target,
                int node_type, struct lock_tree *origin);
struct lock_node *tree_delete_(struct lock_node *root,
                               struct lock_node *target);
void print_tree(struct lock_tree *self);
void print_tree_(struct lock_node *root, int indent);
struct lock_node *get_smallest(struct lock_node *root);
struct lock_node *node_balance(struct lock_node *root);

struct lock_list {
  /* Current Lock Information in linked list */
  int size;
  struct lock_node *head;
};

struct lock_list *list_init(void);
void list_delete(struct lock_list *self);
int list_size(struct lock_list *self);
void list_push(struct lock_list *self, struct lock_node *node);
struct lock_node *list_pop(struct lock_list *self);
void list_print(struct lock_list *self);

#endif
