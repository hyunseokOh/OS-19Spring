# OS - team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---

# Project 1 - Implementing a New System Call
## Table of Contents
- [Edited Files](#edited-files)
- [High Level Design and Implementation](#high-level-design-and-implementation)
- [Test Result](#test-result)
- [Investiagation of Process Tree](#investigation-of-process-tree)
- [Lessons Learned](#lessons-learned)

## Edited Files
- arch/arm64/include/asm/**unistd.h**
  - Increase the number of total system calls
- arch/arm64/inlcude/asm/**unistd32.h**
  - Add a new system call
- include/linux/**syscalls.h**
  - Add an assembly linkage for a new system call 
- kernel/**Makefile**
  - Add a new rule in Makefile
- include/linux/**prinfo.h**
  - Define prinfo struct for each process
- kernel/**ptree.c**
  - Implement ptree system call
  - Search all running processes by DFS algorithm and return the process tree information
- test/**test_ptree.c**
  - Implement testing file for ptree system call
  
**Total Edited LOC: 167**

## High Level Design and Implementation
![schematic](https://user-images.githubusercontent.com/25524539/54747238-ae10c380-4c11-11e9-836d-63085b9e61bb.png)

### Implementation Detail
- Indentation of `ptree` information
    - `task_struct comm` has length as 16, `prinfo comm` has length as 64
    - We have 48 extra spaces for characters in our `prinfo comm`
    - We thought that kernel handles everything includes indentation is much more natural than user handles indentation while printing over loop
    - Thus, we assumed there are no deep processes whose depth exceeds 48 and we padded `\t` in front of `prinfo comm`

## Test Result
### Case 1 - nr value 30
![nr30_cut](https://user-images.githubusercontent.com/25524539/54807604-1c608f00-4cc1-11e9-9afc-a9c6bb8877ce.png)

- Print process tree with **30** process information
- Return value **126** (The number of total process)
- Keep nr value at **30**

---

### Case 2 - nr value 200
![nr200](https://user-images.githubusercontent.com/25524539/54807416-862c6900-4cc0-11e9-9d94-29ec3c14814a.png)

- Print process tree with **entire** process information (**126** in test case)
- Return value **126**
- Change nr value to **126**

---

### Case 3 - nr value 0
![nr0_cut](https://user-images.githubusercontent.com/25524539/54807603-1bc7f880-4cc1-11e9-87fa-4b4df71b49dd.png)

- Return value **-1**
- Keep nr value at **0**


## Investigation of Process Tree
### Swapper
- First process created by the system
- Creates systemd (pid 1) and kthreadd (pid 2) process
- Idle process that runs only when there is no other runnable process
- Represent the state of 'not working'

### Systemd
- System management daemon
- Manages all user processes
- replacement of 'init' system (many linux distributions have adopted it)

![nr30_cut](https://user-images.githubusercontent.com/25524539/54807604-1c608f00-4cc1-11e9-9afc-a9c6bb8877ce.png)


### Kthreadd
- Kernel thread daemon
- Its creation is independent from systemd
- All kernel threads are forked off from kthreadd

![kthreadd](https://user-images.githubusercontent.com/25524539/54807418-862c6900-4cc0-11e9-85d4-b1f8699653fa.png)


## Lessons Learned
- A journey of a thousand miles begins with a single step.
- Kernel has quite a complex structure more than we have expected.
- Step 0 of all development : Check your Hardware!
- It is difficult yet important to use appropriate macros in the kernel source.
