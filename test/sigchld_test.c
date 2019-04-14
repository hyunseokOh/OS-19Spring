#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#define SYSCALL_SET_ROTATION 398
#define SYSCALL_ROTLOCK_READ 399
#define SYSCALL_ROTLOCK_WRITE 400
#define SYSCALL_ROTUNLOCK_READ 401
#define SYSCALL_ROTUNLOCK_WRITE 402

void handler(int sig) {
  printf("child ended\n");
  wait(NULL);
  syscall(SYSCALL_SET_ROTATION, 50);
}

int main(void)
{
  long lock = 0;
  long unlock = 0;
  syscall(SYSCALL_SET_ROTATION, 0);
  signal(SIGCHLD, handler);

  if(fork() == 0)
  {
    sleep(3);
    printf("Child pid is %d\n", getpid());
    printf("Now child will exit and send SIGCHILD\n");
    exit(0);
  }

  lock = syscall(SYSCALL_ROTLOCK_WRITE, 60, 20);
  printf("Parent pid is %d\n", getpid());
  unlock = syscall(SYSCALL_ROTUNLOCK_WRITE, 60, 20);

  printf("Syslock result = %ld\n", lock);
  printf("Sysunlock result = %ld\n", unlock);

  getchar();

  return 0;
}
