# OS - team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 2 - Implementing a Fine-grained Rotation Lock
## Table of Contents
- [Edited Files](#edited-files)
- [High Level Design and Implementation](#high-level-design-and-implementation)
- [Optimization](#optimization)
- [Test Result](#test-result)
- [Lessons Learned](#lessons-learned)

## Edited Files
- arch/arm64/include/asm/**unistd.h**
  - Increase the number of total system calls
- arch/arm64/inlcude/asm/**unistd32.h**
  - Add new system calls
- include/linux/**syscalls.h**
  - Add assembly linkage for each new system call 
- kernel/**Makefile**
  - Add a new rule in Makefile
- include/linux/**rotation.h**
  - Add macros, declarations, and helper functions for rotation lock
- kernel/**rotation.c**
  - Implement rotation lock
- **rotd.c**
  - updates the rotation value in the specific sequence in a fixed frequency
- test/**selector.c**
  - Implement testing file for writer lock
- test/**trial.c**
  - Implement testing file for reader lock

(for additional testing)

- test/**cross.c**
  - Implement 'Banking Example' using rotation lock
- test/**selector_arg.c**
  - Receive degree and range as commandline argument (used for video demo)
- test/**trial_arg.c**
  - Receive degree and range as commandline argument (used for video demo)


**Total Edited LOC: 829**

## High Level Design and Implementation


## Optimization

- Reduce lookup time by masking invalid degrees

  (Motivation)
  - Given only list of locks, we need to iterate over the entire list for each `lock_node` to check if it can grab a lock. This can lead to theoretical worst-case complexity of O(n<sup>2</sup>).
  - However, creating and managing a separate shared data structure for available ranges can lead to lock contention and trickier synchronization problems.

  (Implementation)
  - Therefore, we use `char validRange[360]`, whose scope is bounded to a single method.
  - For every search, we loop over a list and mask off invalid ranges, reducing the complexity to O(360 * n).
  
![optimization1_cropped](https://user-images.githubusercontent.com/22310099/56062882-c99e6280-5da8-11e9-8570-c0a1eef3a810.gif)

- Save boundary values of range into a single `int`
  - Instead of using two variables to represent lower bound and upper bound of a lock range, we save boundary values into a single `int` in the following manner.
  - Macros defined in `rotation.h` are used to update and retrieve actual values.

![optimization2](https://user-images.githubusercontent.com/22310099/56062743-66accb80-5da8-11e9-9978-b5b2511aad20.jpg)  

## Test Result

### Case 1: `./selector 0` + `./trial 0 & ./trial 1`
* selector: writer lock of range [0, 180]
* trial-0, trial-1: reader lock of range [0, 180]

![test1](https://user-images.githubusercontent.com/22310099/56063708-0703ef80-5dab-11e9-960d-d843a64b6c54.gif)

* Selector and trials grab locks **only** when the degree is within [0, 180]

### Case 2: `./selector_arg 1 30 1` + `./trial 0 & ./trial 1 & ./trial 2`
* selector: writer lock of range [29, 31]
* trial-0, trial-1, trial-2: reader lock of range [0, 180]

![test2](https://user-images.githubusercontent.com/22310099/56062927-ea66b800-5da8-11e9-8657-ef93dbcb6f4e.gif)

* Selector and trials grab locks **only** when the degree is 30
* Trials **cannot** grab locks because **write lock request [29, 31] is waiting** and it **intersects** with read lock request of [0, 180]

### Case 3: `./selector_arg 1 30 1` + `./trial_arg 0 90 90 & ./trial_arg 1 270 90`
* selector: writer lock of range [29, 31]
* trial-0: reader lock of range [0, 180]
* trial-1: reader lock of range [180, 360]

![test3](https://user-images.githubusercontent.com/22310099/56062928-eb97e500-5da8-11e9-883e-7f9b01ba74ed.gif)

* Selector and trial-0 grab locks **only** when the degree is 30
* Trial-0 **cannot** grab locks in (30, 180] because **write lock request [29, 31] is wating** and it **intersects** with read lock request of [0, 180]
* Trial-1 grabs locks when the degree is within [180, 360]. It **does not intersect with wating write request of range [29, 31]**

## Lessons Learned
- Synchronization problems are disasters.
- Using a lock in appropriate position is significant.
- Design review meeting is quite important for progressing the project.
- What a sequential creature human is!
- Expect the unexpected? Handle the unexpected!
- Let's figure out what is in scheduler's head at next project.
- (Misc.) There are cool large prime numbers such as 49999991 and 123456891 :smile:
