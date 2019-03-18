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
#include <linux/sched/signal.h> /* for_each_process(p) macro */
#include <linux/sched/task.h>   /* task_list lock */
#include <linux/slab.h>         /* kmalloc, kfree */
#include <linux/string.h>       /* strncpy */
#include <linux/syscalls.h>     /* SYSCALL_DEFINE */
#include <linux/uaccess.h>      /* user-space access */

#include <uapi/asm-generic/errno-base.h> /* Error codes */

/*
 * save_prinfo: stores the information of a given task(process) into prinfo
 * buffer
 *
 * struct task_struct* task: process(in task_struct) whose information is to be
 * saved
 * struct prinfo* buf: buffer for the process data
 * int i: index for the buffer designating save location
 * returns 0 if success, else returns error code
 * TODO (taebum): maybe void return is enough?
 */
int save_prinfo(struct task_struct* task, struct prinfo* buf, int i,
                int nrValue) {
  struct task_struct* first_child;
  struct task_struct* next_sibling;

  first_child = list_entry(&(task->children), struct task_struct, children);
  next_sibling = list_entry(&(task->sibling), struct task_struct, sibling);

  buf[i].state = task->state;
  buf[i].pid = task->pid;
  buf[i].parent_pid = task->real_parent->pid;
  /* TODO (taebum)
   * For child and sibling, how can we check whether no child (leaf)
   *  or no more sibling (tail of task_struct list)
   *
   * I think we must handle for each case
   */
  buf[i].first_child_pid = first_child->pid;
  buf[i].next_sibling_pid = next_sibling->pid;
  buf[i].uid = (uint64_t)task->real_cred->uid.val;
  strncpy(buf[i].comm, task->comm, TASK_COMM_LEN);

  return 0;
}

void ptreeTraverse(struct prinfo* buf, int nrValue, int* cnt) {
  struct task_struct* task;
  /*
   * TODO (taebum) 
   * Current traverse just traverse without a hierachy
   *  For example, systemd-journal comes much later than systemd
   *  I think we should use list_for_each_entry?
   */
  for_each_process(task) {
    if (*cnt >= nrValue) {
      return;
    }
    save_prinfo(task, buf, *cnt, nrValue);
    *cnt = *cnt + 1;
  }
}

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr) {
  /* Compile stage seems to give below warning if a variable is declared in the
   * middle of a function
   * "warning: ISO C90 forbids mixed declarations and code
   * [-Wdeclaration-after-statement]""
   */

  int nrValue; /* nr length */
  int cnt = 0;
  struct prinfo* kernelBuf = NULL;

  /* EINVAL check */
  if (buf == NULL || nr == NULL) {
    return -EINVAL;
  }

  /* read nr data from user space */
  if (copy_from_user(&nrValue, nr, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    printk("We have failed in copying nr to kernel space\n");
    return -EFAULT;
  }

  if (nrValue < 1) {
    return -EINVAL;
  }

  kernelBuf =
      (struct prinfo*)kmalloc(sizeof(struct prinfo) * nrValue, GFP_KERNEL);
  if (kernelBuf == NULL) {
    /* allocation failed */
    printk("We have failed in kmalloc for buf\n");
    return -ENOMEM;
  }

  /* task lock */
  read_lock(&tasklist_lock);

  /* actual traversal goes here */
  ptreeTraverse(kernelBuf, nrValue, &cnt);

  /* unlock */
  read_unlock(&tasklist_lock);

  if (copy_to_user(buf, kernelBuf, sizeof(struct prinfo) * nrValue)) {
    /* copying kernel to user failed */
    printk("We have failed in copying buf back to user space\n");
    return -EFAULT;
  }

  if (copy_to_user(nr, &cnt, sizeof(int))) {
    /* copying kernel to user failed */
    printk("We have failed in copying nr back to user space\n");
    return -EFAULT;
  }

  kfree(kernelBuf);

  return (long)cnt;
}
