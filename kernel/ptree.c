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

#include <linux/prinfo.h>     /* prinfo struct */
#include <linux/sched.h>      /* task struct */
#include <linux/sched/task.h> /* task_list lock */
#include <linux/syscalls.h>   /* SYSCALL_DEFINE */
#include <linux/uaccess.h>    /* user-space access */

#include <uapi/asm-generic/errno-base.h> /* Error codes */

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr) {
  int nrValue; /* nr length */

  /* EINVAL check */
  if (buf == NULL || nr == NULL) {
    return -EINVAL
  }

  /* read nr data from user space */
  if (copy_from_user(&nrValue, nr, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (nrValue < 1) {
    return -EINVAL;
  }

  return 0;
}
