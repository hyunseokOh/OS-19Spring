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

#include <linux/list.h>       /* linked likst usage */
#include <linux/prinfo.h>     /* prinfo struct */
#include <linux/sched.h>      /* task struct */
#include <linux/sched/task.h> /* task_list lock */
#include <linux/slab.h>       /* kmalloc, kfree */
#include <linux/syscalls.h>   /* SYSCALL_DEFINE */
#include <linux/uaccess.h>    /* user-space access */

#include <uapi/asm-generic/errno-base.h> /* Error codes */

#include <linux/sched/signal.h> /* for_each_process(p) macro */

/*
 * save_prinfo: stores the information of a given task(process) into prinfo
 * buffer
 *
 * struct task_struct* task: process(in task_struct) whose information is to be
 * saved
 * struct prinfo* buf: buffer for the process data
 * int index: index for the buffer designating save location
 * returns 0 if success, else returns error code
 */
int save_prinfo(struct task_struct* task, struct prinfo* buf, int index) {
  int i;

  buf[index].state = task->state;
  buf[index].pid = task->pid;
  buf[index].parent_pid = task->real_parent->pid;
  buf[index].first_child_pid =
      list_entry(&(task->children), struct task_struct, children)->pid;
  buf[index].next_sibling_pid =
      list_entry(&(task->sibling), struct task_struct, sibling)->pid;
  buf[index].uid = (uint64_t)task->real_cred->uid
                       .val;  // uid.val is uid32_t, so we need to type cast it

  /* TODO: Replace naive for loop with a string copy mechanism provided within
   * the kernel source code */
  for (i = 0; i < TASK_COMM_LEN; i++) {
    buf[index].comm[i] = task->comm[i];
    if (task->comm[i] == '\n') {
      break;
    }
  }

  return 0;
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
  struct task_struct* p;

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
  for_each_process(p) {
    if (cnt < nrValue) {
      save_prinfo(p, kernelBuf, cnt);
      ++cnt;
    } else {
      break;
    }
  }

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

  return 0;
}
