# OS - team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seong-hoon Seo

---

# Project 1 - Implementing a New System Call
## Table of Contents
- [Edited Files](#edited-files)
- [High Level Design and Implementation](#high-level-design-and-implementation)
- [Investiagation of Process Tree](#investigation-of-process-tree)
- [Test Result](#test-result)
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
  
Total Edited LOC: 153 

## High Level Design and Implementation

(Schematic of program structure)

## Investigation of Process Tree

## Test Result
### Case 1 - nr value 10
(Photo)
### Case 2 - nr value 200
(Photo)
### Case 3 - nr value 0
(Photo)

## Lessons Learned
- A journey of a thousand miles begins with a single step.
- Kernel has a quite complex structure more than we have expected.
- Step 0 of development : Hardware checking
- It is hard and important to find appropriate macros in the kernel source.
- 
