# os-team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 3 - Weighted Round Robin Scheduler 

# Implementation Detail

## Modified/Implemented Functions

### Except `kernel/sched/wrr.c`

1. `arch/arm64/Kconfig`
    - Add `CONFIG_SCHED_WRR`, `CONFIG_GONGLE_DEBUG`, `CONFIG_WRR_BALANCE_TEST`
2. `arch/arm64/include/asm/{unistd.h,unistd32.h}`, `include/inlux/syscalls.h`
    - syscall related (easy now :smile:)
3. `include/uapi/linux/sched.h`
    - Define `SCHED_WRR 7`
4. `include/linux/sched.h`
    - Add `struct sched_wrr_entity`
    - Add `sched_wrr_entity` and `forbidden_allowed` field to `task_struct`
5. `INIT_TASK` of `include/linux/init_task.h`
    - Default `sched_wrr_entity` setting fof `init_task` (all tasks forked from this)
6. `rt_sched_class` in `kernel/sched/rt.c`
    - Set `.next` as `&wrr_sched_class` if `CONFIG_SCHED_WRR` is active
7. `kernel/sched/sched.h`
    - Add `struct wrr_rq` and some helper functions like `wrr_policy`
    - Modify `valid_policy` to check `SCHED_WRR`
    - Add `wrr` to `struct rq`
8. `__sched_fork` in `kernel/sched/core.c`
    - Init `struct list_head wrr_node`
    - Set weight, time_slice
9. `sched_fork` in `kernel/sched/core.c`
    - If `sched_reset_on_fork == 1` set `wrr` weight as default weight
    - If policy is `SCHED_WRR`, set `wrr_sched_class` as `sched_class`
10. `__setscheduler` in `kernel/sched/core.c`
    - If policy is `SCHED_WRR`, set `wrr_sched_class` as `sched_class`
11. `__setscheduler_params` in `kernel/sched/core.c`
    - Do not modify `rt_priority` if `policy == SCHED_WRR`
12. `__sched_setscheduler` in `kernel/sched/core.c`
    - If policy is `SCHED_WRR`, do not check contition which is related to `sched_priority`
    - `sched_setscheduler` should work independently of `sched_priority`
13. `do_sched_setscheduler` in `kernel/sched/core.c`
    - Handle `FORBIDDEN_WRR_QUEUE`
14. `sched_setaffinity` in `kernel/sched/core.c`
    - Handle `FORBIDDEN_WRR_QUEUE`
15. `sys_sched_set_weight` in `kernel/sched/core.c`
    - syscall implementation
16. `sys_sched_get_weight` in `kernel/sched/core.c`
    - syscall implementation
17. `sys_sched_get_balance` in `kernel/sched/core.c`
    - extra syscall implementation (not in project specification)
    - For getting each cpu's `wrr_rq`'s weight to the user space
18. `sched_init` in `kernel/sched/core.c`
    - Insert `init_wrr_rq`
19. `include/linux/shced/wrr.h`
    - Useful MACROS for `SCHED_WRR`

### `kernel/sched/wrr.c`

It does not contain debug-related functions and some trivial functions.

1. `get_target_cpu`
    - Get cpu number whose `wrr_rq`'s weight sum is min/max
2. `init_wrr_rq`
    - Initialize given `wrr_rq`
3. `requeue_task`
    - move head node of the `wrr_rq` to tail of queue
4. `enqueue_task_wrr` and `enqueue_wrr_entity`
    - Enqueue given task into `wrr_rq`
5. `dequeue_task_wrr` and `dequeue_wrr_entity`
    - Dequeue given task from `wrr_rq`
6. `pick_next_task_wrr`
    - Called from scheduler, when it tries to find the next task to run
    - Return `NULL` if current `wrr_rq` of `rq` is empty
    - Return head of `wrr_rq` if current `wrr_rq` of `rq` is not empty (and call `put_prev_task` for prev task)
7. `select_task_rq_wrr`
    - Deciding which CPU a task should be assigned to (min weight sum)
8. `set_cpus_allowed_wrr`
    - Change `cpus_allowed` of given task
    - Prevent `FORBIDDEN_WRR_QUEUE` be set
9. `task_tick_wrr`
    - Called from `scheduler_tick`
    - Decrease time slice, and requeue & reschedule task if time slice becomes zero
    
## How to Keep the Single `wrr_rq` Empty

In the specification, it says that
> Tasks will manually be switched to use `SCHED_WRR` using system call `sched_setscheduler()`.  

We assumed that it denotes `sched_setscheduler` system call is the only way to change sched policy of task.  
Moreover, we investigated that assigned cpu could be modified via system call `sched_setaffinity()`.  
Thus, we modified both system calls' logic as follows.

### `sched_setscheduler`

For `sched_setscheduler`, the call stack becomes
```
sys_sched_setscheduler
do_sched_setscheduler
sched_setscheduler
_sched_setscheduler
__sched_setscheduler
...
```
We modified `do_sched_setscheduler` function.  
It checks whether scheduler policy becomes `Other -> SCHED_WRR` or `SCHED_WRR -> Other`.  
The former case, if `FORBIDDEN_WRR_QUEUE` is allowed for current task, it masks out that cpu number via `set_cpus_allowed_ptr`.  
Moreover, it saves information that `FORBIDDEN_WRR_QUEUE` was allowed for this task before scheduler policy conversion in `forbidden_allowed` field of `struct task_struct`.  
However, if `FORBIDDEN_WRR_QUEUE` is the only allowed cpu for given task, it returns error (cannot change sched policy).  
The latter case, it makes `FORBIDDEN_WRR_QUEUE` allowed if `forbidden_allowed` of task is 1 (true).

### `sched_setaffinity`

If current task's policy is `SCHED_WRR`, it masks out `FORBIDDEN_WRR_QUEUE` from `new_mask`.  
After that, if `new_mask`'s weight is 0 (no cpu in mask), it returns error (cannot set affinity).

# Investigate

## Test Programs

### How to prepare test files

To compile all test files, type following shell commands in project.
```
$ cd test
$ mkdir bin
$ make
```
Then all executable binary files are generated in `test/bin`, so now you can just copy them to rpi3.

### List of tests

The following test names are based on executable binary files
1. `simple`
    - Check simple conversion of sched policy to `SCHED_WRR`
    - It converts sched policy via `sched_setscheduler`, and calls `sched_get_weight`
2. `io`
    - Check simple conversion of sched policy to `SCHED_WRR` + I/O task
    - It converts sched policy and waits for I/O (`getchar()`)
3. `sleep`
    - Check simple conversion of sched policy to `SCHED_WRR` + sleep task
    - It converts sched policy and sleeps for 2 seconds
4. `error`
    - Basic functionality tests
    - It checks following functionalities
        1. Error return when call `sched_get_weight` with negative `pid` number
        2. Error return when call `sched_get_weight` with non-existing `pid` number
        3. Whether `sched_get_weight` for current process returns 10
        4. Error return when call `sched_set_weight` with non-existing `pid` number
        5. Error return when call `sched_set_weight` with negative `pid` number
        6. Error return when call `sched_set_weight` with invalid weight
        7. Error return when call `sched_set_weight` to task whose policy is not `SCHED_WRR` (could return `EPERM` in userside)
        8. Success to migrate task's cpu to number `3` (fobidden cpu) via `sched_setaffinity`
        9. Error return when call `sched_setscheduler` for task which is allowed only for cpu number `3` (forbidden cpu)
        10. Success to allow task's cpu number to all via `sched_setaffinity`
        11. Whether cpu of task change when call `sched_setscheduler` if task is on forbidden cpu
        12. Success to modify weight when call `sched_set_weight` `10 -> 5` (decrease)
        13. Check whether forked process have same weight with parent
        14. Try to increase weight `5 -> 20`. Success when root, fail when user
    - For testing in user mode, run `$ ./copy2user.sh` in root mode and login with `ID: owner PW: tizen`
5. `fork`
    - Check fork task
    - It forks 3 times after conversion of sched policy to `SCHED_WRR`, and run factorization task
6. `trial`
    - One of the main test
    - It takes two command line args, `nproc` and `equal_weight`
    - `./trial 10 1` means
        - Fork 10 child processes
        - Each process have equal weights
    - It iterates over weight value from `1` to `20`
    - At each weight value, it checks elapsed time (in real time, not cpu time) of the last child process
    - It checks elapsed time for 10 iterations, and writes the result into text file (filename becomes `trial_$(nproc)_$(equal_weight).txt`)
7. `random`
    - One of the main test
    - It takes single command line args, `nproc`
    - `./random 10` means
        - Fork 10 child processes
    - It iterates over weight value from `1` to `20`
    - At each weight value, it checks elapsed time (in real time, not cpu time) of the last child process
    - It checks elapsed time for 10 iterations, and writes the result into text file (filename becomes `random_$(nproc).txt`)
    - Difference between `trial` and `random` is `random` test set other processes' weight as random number
8. `balance`
    - One of the main test
    - It takes single command line args, `nproc`
    - `./balance 10` means
        - Test `balance` with 10 child processes fork
    - It records balance factor of each `wrr_rq` every second (using extra system call `sched_get_balance`)
    - It writes the balance factor history into text file (filename becomoes `balance_$(nproc).txt`)
    
## Test Results


