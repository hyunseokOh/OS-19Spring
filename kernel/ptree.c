/*
 * linux/kernel/ptree.c
 *
 * Seoul National University Operating System 2019 Spring
 * Team 8
 *
 */

/*
 * 'ptree.c' contains the help-routines for the 'ptree' system call
 *
 */

#include <linux/list.h>         /* linked likst usage */
#include <linux/prinfo.h>       /* prinfo struct */
#include <linux/sched.h>        /* task struct */
#include <linux/sched/task.h>   /* task_list lock */
#include <linux/slab.h>         /* kmalloc, kfree */
#include <linux/string.h>       /* strncpy */
#include <linux/syscalls.h>     /* SYSCALL_DEFINE */
#include <linux/uaccess.h>      /* user-space access */

#include <uapi/asm-generic/errno-base.h> /* Error codes */

/*
 * save_prinfo: stores the information of a given task(process) in the prinfo
 * buffer
 *
 * struct task_struct* task: process(in the form of task_struct) whose information is to be saved
 * struct prinfo* buf: buffer for the process data
 * int i: index for the buffer designating save location
 * int indent: indentation number for comm
 */
void save_prinfo(struct task_struct* task, struct prinfo* buf, int i,
                 int indent) {
  struct task_struct* first_child;
  struct task_struct* next_sibling;
  int j;

  buf[i].state = (int64_t)task->state;
  buf[i].pid = task->pid;
  buf[i].parent_pid = task->real_parent->pid;

  if (list_empty(&(task->children))) {
    /* leaf */
    buf[i].first_child_pid = 0;
  } else {
    first_child =
        list_first_entry(&(task->children), struct task_struct, sibling);
    buf[i].first_child_pid = first_child->pid;
  }

  if (list_is_last(&(task->sibling), &(task->real_parent->children))) {
    /* tail */
    buf[i].next_sibling_pid = 0;
  } else {
    next_sibling =
        list_first_entry(&(task->sibling), struct task_struct, sibling);
    buf[i].next_sibling_pid = next_sibling->pid;
  }
  buf[i].uid =(int64_t)task->real_cred->uid.val;

  /* indentation pad */
  for (j = 0; j < indent; j++) {
    buf[i].comm[j] = '\t';
  }
  strncpy(buf[i].comm + indent, task->comm, TASK_COMM_LEN);

  return;
}

void ptreeTraverse_(struct task_struct* task, struct prinfo* buf, int nrValue,
                    int* cnt, int* access, int indent) {
  /* traverse current task and all children */
  struct task_struct* traverse;

  /* total access increase */
  *access = *access + 1;

  if (*cnt < nrValue) {
    /* copy */
    save_prinfo(task, buf, *cnt, indent);
    *cnt = *cnt + 1;
  }

  /* traverse */
  list_for_each_entry(traverse, &(task->children), sibling) {
    ptreeTraverse_(traverse, buf, nrValue, cnt, access, indent + 1);
  }
}

int ptreeTraverse(struct prinfo* buf, int nrValue, int* cnt) {
  /*
   * Outermost wrapper for actual traversal
   */
  struct task_struct* task;
  int access = 0;

  task = &init_task;
  ptreeTraverse_(task, buf, nrValue, cnt, &access, 0);
  return access;
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr) {
  /* Compile stage seems to give below warning if a variable is declared in the
   * middle of a function
   * "warning: ISO C90 forbids mixed declarations and code
   * [-Wdeclaration-after-statement]""
   */

  int nrValue; /* nr length */
  int cnt = 0;
  int access;
  struct prinfo* kernelBuf = NULL;

  /* EINVAL check */
  if (buf == NULL || nr == NULL) {
    return -EINVAL;
  }

  /* read nr data from user space */
  if (copy_from_user(&nrValue, nr, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (nrValue < 1) {
    return -EINVAL;
  }

  kernelBuf =
      (struct prinfo*)kmalloc(sizeof(struct prinfo) * nrValue, GFP_KERNEL);
  if (kernelBuf == NULL) {
    /* allocation failed */
    return -ENOMEM;
  }

  /* task lock */
  read_lock(&tasklist_lock);

  /* actual traversal goes here */
  access = ptreeTraverse(kernelBuf, nrValue, &cnt);

  /* unlock */
  read_unlock(&tasklist_lock);

  if (copy_to_user(buf, kernelBuf, sizeof(struct prinfo) * cnt)) {
    /* copying kernel to user failed */
    return -EFAULT;
  }

  if (copy_to_user(nr, &cnt, sizeof(int))) {
    /* copying kernel to user failed */
    return -EFAULT;
  }

  kfree(kernelBuf);

  return (int64_t)access;
}
