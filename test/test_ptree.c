#include <stdint.h> /* int64_t */
#include <stdio.h>
#include <stdlib.h>      /* malloc */
#include <string.h>      /* atoi */
#include <sys/syscall.h> /* sycall method */
#include <sys/types.h>   /* declaration of pid_t, etc. */
#include <unistd.h>

struct prinfo {
  int64_t state;          /* current state of process */
  pid_t pid;              /* process id */
  pid_t parent_pid;       /* process id of parent */
  pid_t first_child_pid;  /* pid of oldest child */
  pid_t next_sibling_pid; /* pid of younger sibling */
  int64_t uid;            /* user id of process owner */
  char comm[64];          /* name of program executed */
};

int main(int argc, char *argv[]) {
  int nr;
  int nrBefore;
  struct prinfo *buf = NULL;

  if (argc != 2) {
    printf("You need to pass nr as a command line argument!\n");
    return 0;
  }

  /* TODO: Error Handling for the case argument is not a number */

  nr = atoi(argv[1]);
  nrBefore = nr;

  if (nr < 0) {
    printf("nr should be a positive number\n");
    return 0;
  }

  printf("Requested nr: %d\n", nr);

  buf = (struct prinfo *)malloc(sizeof(struct prinfo) * nr);

  long int syscallResult =
      syscall(398, buf, &nr);  // TODO: Check whether this syntax is correct

  for (int i = 0; i < nr; i++) {
    struct prinfo p = buf[i];
    printf("%s,%d,%lld,%d,%d,%d,%lld\n", p.comm, p.pid, p.state, p.parent_pid,
           p.first_child_pid, p.next_sibling_pid, p.uid);
  }

  printf("value of nr before: %d, after: %d\n", nrBefore, nr);
  printf("System call sys_ptree returned %ld\n", syscallResult);

  printf("TEST END\n");

  free(buf);
  return 0;
}
