# os-team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 3 - Weighted Round Robin Scheduler 

# Implementation Detail

## Modified/Implemented Functions

1. `arch/arm64/Kconfig`
    - Add `CONFIG_SCHED_WRR` and `CONFIG_GONGLE_DEBUG`
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
9. `sched_fork` in `kernel/sched/core.c`
10. `__setscheduler` in `kernel/sched/core.c`
11. `__sched_setscheduler` in `kernel/sched/core.c`
12. `do_sched_setscheduler` in `kernel/sched/core.c`
13. `sched_setaffinity` in `kernel/sched/core.c`
14. `sys_sched_set_weight` in `kernel/sched/core.c`
15. `sys_sched_get_weight` in `kernel/sched/core.c`
16. `sched_init` in `kernel/sched/core.c`
17. `include/linux/shced/wrr.h`
    - Useful MACROS for `SCHED_WRR`

## How to Keep the Single wrr_rq Empty (CPU 3 of rpi3)

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
6. `test`
    - One of the main test
    - It takes two command line args, `nproc` and `equal_weight`
    - `./test 10 1` means
        - Fork 10 child processes
        - Each process have equal weights
    - It iterates over weight value from `1` to `20`
    - At each weight value, it checks elapsed time (in real time, not cpu time) of the last child process
    
## Test Results
