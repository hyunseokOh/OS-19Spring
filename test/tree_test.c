#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "tree.h"

static pid_t pid = 0;

int check_increasing_order(struct lock_tree *tree);
void check_increasing_order_(struct lock_node *root,
                             struct lock_node **previous, int *result);

int main(void) {
  srand(time(NULL));
  struct lock_tree *tree;
  struct lock_list *list;
  int degree;
  int range;
  tree = tree_init();

  for (int i = 0; i < 20; i++) {
    degree = rand() % 360;
    range = rand() % 179 + 1;

    tree_insert(tree, pid++, degree, range, READER);
  }

  print_tree(tree);
  printf("\n\n");
  list = tree_find(tree, 30);
  print_tree(tree);

  printf("List Size = %d\n", list_size(list));

  if (check_increasing_order(tree)) {
    printf("Sorted Test Success\n");
  } else {
    printf("Sorted Test Failed\n");
  }

  tree_delete(tree);
}

int check_increasing_order(struct lock_tree *tree) {
  int result = 1;
  struct lock_node *previous = NULL;

  check_increasing_order_(tree->root, &previous, &result);

  return result;
}

void check_increasing_order_(struct lock_node *root,
                             struct lock_node **previous, int *result) {
  if (root == NULL) {
    return;
  }

  check_increasing_order_(root->left, previous, result);
  if (*previous != NULL) {
    if (node_compare(*previous, root) >= 0) {
      *result = 0;
    }
  }
  *previous = root;
  check_increasing_order_(root->right, previous, result);
}
