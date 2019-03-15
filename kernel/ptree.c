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

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr) {
  int nrValue; /* nr length */
  int64_t result;
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

  kernelBuf = (struct prinfo*)kmalloc(sizeof(struct prinfo) * nrValue, GFP_KERNEL);
  if (kernelBuf == NULL) {
    /* allocation failed */
    return -ENOMEM;
  }

  /* task lock */
  read_lock(&tasklist_lock);

  /* actual traversal goes here */

  /* unlock */
  read_unlock(&tasklist_lock);

  if (copy_to_user(buf, kernelBuf, sizeof(struct prinfo) * nrValue)) {
    /* copying kernel to user failed */
    return -EFAULT;
  }

  kfree(kernelBuf);
  return 0;
}
