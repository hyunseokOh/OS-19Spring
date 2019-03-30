#include "tree.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* node related */
struct lock_node *node_init(pid_t pid, int r_low, int r_high, int node_type) {
  struct lock_node *node = NULL;
  node = (struct lock_node *)malloc(sizeof(struct lock_node));

  node->pid = pid;
  node->range[0] = r_low;
  node->range[1] = r_high;
  node->node_type = node_type;
  node->height = 1;
  node->left = NULL;
  node->right = NULL;
  node->next = NULL;

  return node;
}

void node_delete(struct lock_node *node) {
  if (node == NULL) {
    return;
  }
  free(node);
}

void print_node(struct lock_node *node) {
  if (node == NULL) {
    return;
  }
  if (node->node_type == WRITER) {
    printf("PID: %d, range = [%d, %d], type = WRITER, height = %d\n", node->pid,
           node->range[0], node->range[1], node->height);
  } else {
    printf("PID: %d, range = [%d, %d], type = READER, height = %d\n", node->pid,
           node->range[0], node->range[1], node->height);
  }
}

int node_compare(struct lock_node *self, struct lock_node *other) {
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

struct lock_node *right_rotate(struct lock_node *node) {
  struct lock_node *left = node->left;
  struct lock_node *leftRight = left->right;

  left->right = node;
  node->left = leftRight;

  node->height = 1 + MAX_HEIGHT(node->left, node->right);
  left->height = 1 + MAX_HEIGHT(left->left, left->right);
}
struct lock_node *left_rotate(struct lock_node *node) {
  struct lock_node *right = node->right;
  struct lock_node *rightLeft = right->left;

  right->left = node;
  node->right = rightLeft;

  node->height = 1 + MAX_HEIGHT(node->left, node->right);
  right->height = 1 + MAX_HEIGHT(right->left, right->right);
}
struct lock_node *double_right_rotate(struct lock_node *node) {
  struct lock_node *left = node->left;
  node->left = left_rotate(left);
  return right_rotate(node);
}
struct lock_node *double_left_rotate(struct lock_node *node) {
  struct lock_node *right = node->right;
  node->right = right_rotate(right);
  return left_rotate(node);
}

/* tree related */
struct lock_tree *tree_init(void) {
  struct lock_tree *tree = NULL;
  tree = (struct lock_tree *)malloc(sizeof(struct lock_tree));
  tree->root = NULL;

  return tree;
}

void tree_delete(struct lock_tree *self) {
  struct lock_node *node;
  if (self == NULL) {
    return;
  }

  node = self->root;
  while (node != NULL) {
    self->root = tree_delete_(self->root, node);
    free(node);
    node = self->root;
  }
  free(self);
}

int tree_insert(struct lock_tree *self, pid_t pid, int degree, int range,
                int node_type) {
  struct lock_node *target = NULL;
  int r_low;
  int r_high;
  if (self->root != NULL && self->root->node_type != node_type) {
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
  target = node_init(pid, r_low, r_high, node_type);
  self->root = tree_insert_(self->root, target);
  return 0;
}

struct lock_node *tree_insert_(struct lock_node *root,
                               struct lock_node *target) {
  if (root == NULL) {
    return target;
  }

  if (node_compare(root, target) < 0) {
    /* target is greater, go right */
    root->right = tree_insert_(root->right, target);
  } else {
    root->left = tree_insert_(root->left, target);
  }

  root->height = 1 + MAX_HEIGHT(root->left, root->right);
  root = node_balance(root);

  return root;
}

struct lock_node *node_balance(struct lock_node *root) {
  int leftHeight;
  int rightHeight;

  leftHeight = height(root->left);
  rightHeight = height(root->right);

  if (leftHeight - rightHeight == 2) {
    /* left heavy */
    if (height(root->left->right) > height(root->left->left)) {
      /* double rotation */
      root = double_right_rotate(root);
    } else {
      /* single rotation */
      root = right_rotate(root);
    }
  }

  if (rightHeight - leftHeight == 2) {
    if (height(root->right->left) > height(root->right->right)) {
      /* double rotation */
      root = double_left_rotate(root);
    } else {
      /* single rotation */
      root = left_rotate(root);
    }
  }
  return root;
}

struct lock_node *tree_delete_(struct lock_node *root,
                               struct lock_node *target) {
  struct lock_node *smallest;
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
    root->left = tree_delete_(root->left, target);
  } else if (compare == -1) {
    root->right = tree_delete_(root->right, target);
  } else {
    if (root->left == NULL && root->right == NULL) {
      return NULL;
    } else if (root->left == NULL && root->right != NULL) {
      root = root->right;
    } else if (root->left != NULL && root->right == NULL) {
      root = root->left;
    } else {
      smallest = get_smallest(root->right);
      root->right = tree_delete_(root->right, smallest);
      smallest->right = root->right;
      smallest->left = root->left;
      root = smallest;
    }
  }
  root->height = 1 + MAX_HEIGHT(root->left, root->right);
  root = node_balance(root);
  return root;
}

struct lock_list *tree_find(struct lock_tree *self, int degree) {
  if (self->root == NULL) {
    return NULL;
  }
  struct lock_list *result = list_init();
  if (degree < 180) {
    self->root = tree_find_(self->root, degree, &result, self->root->node_type);
    self->root =
        tree_find_(self->root, degree + 360, &result, self->root->node_type);
  } else if (degree > 180) {
    self->root = tree_find_(self->root, degree, &result, self->root->node_type);
    self->root =
        tree_find_(self->root, degree - 360, &result, self->root->node_type);
  } else {
    /* just find 180 */
    self->root = tree_find_(self->root, degree, &result, self->root->node_type);
  }

  return result;
}

struct lock_node *tree_find_(struct lock_node *root, int degree,
                             struct lock_list **result, int node_type) {
  int r_low;
  int r_high;
  struct lock_node *smallest;
  if (root == NULL) {
    return NULL;
  }

  r_low = root->range[0];
  r_high = root->range[1];

  if (r_low <= degree && degree <= r_high) {
    /* found */
    /* push, delete and go on */
    if (node_type == WRITER) {
      if (list_size(*result) == 0) {
        list_push(*result, root);
        root = tree_delete_(root, root);
        root = tree_find_(root, degree, result, node_type);
      }
    } else {
      list_push(*result, root);
      root = tree_delete_(root, root);
      root = tree_find_(root, degree, result, node_type);
    }
  } else {
    if (degree < r_low) {
      /* search left */
      root->left = tree_find_(root->left, degree, result, node_type);
    } else {
      root->right = tree_find_(root->right, degree, result, node_type);
    }
  }
  return root;
}

void print_tree(struct lock_tree *self) { print_tree_(self->root, 0); }
void print_tree_(struct lock_node *root, int indent) {
  if (root == NULL) {
    return;
  }

  print_tree_(root->left, indent + 1);
  for (int i = 0; i < indent; i++) printf(" ");
  print_node(root);
  print_tree_(root->right, indent + 1);
}

struct lock_node *get_smallest(struct lock_node *root) {
  while (root->left != NULL) {
    root = root->left;
  }
  return root;
}

/* list related */
struct lock_list *list_init(void) {
  struct lock_list *list = (struct lock_list *)malloc(sizeof(struct lock_list));

  list->size = 0;
  list->head = NULL;

  return list;
}

void list_delete(struct lock_list *self) {
  struct lock_node *node;
  if (self == NULL) {
    return;
  } else {
    while ((node = list_pop(self)) != NULL) {
      free(node);
    }
    free(self);
  }
}

int list_size(struct lock_list *self) {
  if (self == NULL) {
    return 0;
  } else {
    return self->size;
  }
}

void list_push(struct lock_list *self, struct lock_node *node) {
  struct lock_node *traverse;
  if (self == NULL) {
    return;
  }

  if (self->head == NULL) {
    self->head = node;
  } else {
    traverse = self->head;
    while (traverse->next != NULL) {
      traverse = traverse->next;
    }
    traverse->next = node;
  }
  self->size++;
}

struct lock_node *list_pop(struct lock_list *self) {
  struct lock_node *node;
  if (self == NULL) {
    return NULL;
  }
  if (self->head != NULL) {
    node = self->head;
    self->head = self->head->next;
    self->size--;
  } else {
    return NULL;
  }
}
