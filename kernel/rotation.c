#include <linux/linkage.h>
#include <linux/printk.h>
#include <linux/rotation.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <uapi/asm-generic/errno-base.h>

int64_t set_rotation(int degree) {
  /* For first test, just print degree */
  printk("GONGLE: set_rotation syscall get degree as %d\n", degree);
  return 0;
}

int64_t rotlock_read(int degree, int range) { return 0; }
int64_t rotlock_write(int degree, int range) { return 0; }
int64_t rotunlock_read(int degree, int range) { return 0; }
int64_t rotunlock_write(int degree, int range) { return 0; }

asmlinkage long sys_set_rotation(int degree) {
  int kernelDegree;
  if (copy_from_user(&kernelDegree, &degree, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }
  set_rotation(kernelDegree);
  return 0;
}

asmlinkage long sys_rotlock_read(int degree, int range) {
  int kernelDegree;
  int kernelRange;

  if (copy_from_user(&kernelDegree, &degree, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (copy_from_user(&kernelRange, &range, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }
  return 0;
}

asmlinkage long sys_rotlock_write(int degree, int range) {
  int kernelDegree;
  int kernelRange;

  if (copy_from_user(&kernelDegree, &degree, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (copy_from_user(&kernelRange, &range, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }
  return 0;
}

asmlinkage long sys_rotunlock_read(int degree, int range) {
  int kernelDegree;
  int kernelRange;

  if (copy_from_user(&kernelDegree, &degree, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (copy_from_user(&kernelRange, &range, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }
  return 0;
}

asmlinkage long sys_rotunlock_write(int degree, int range) {
  int kernelDegree;
  int kernelRange;

  if (copy_from_user(&kernelDegree, &degree, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }

  if (copy_from_user(&kernelRange, &range, sizeof(int))) {
    /* EFAULT occur (cannot access) */
    return -EFAULT;
  }
  return 0;
}
