# OS - team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 2 - Implementing a Fine Grained Rotation Lock
## Table of Contents
- [Edited Files](#edited-files)
- [High Level Design and Implementation](#high-level-design-and-implementation)
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
- kernel/**rotation.c**
  - Implement rotation lock
- **rotd.c**
  - updates the rotation value in the specific sequence in a fixed frequency
- test/**selector.c**
  - Implement testing file for writer lock
- test/**trial.c**
  - Implement testing file for reader lock


**Total Edited LOC: ???**

## High Level Design and Implementation

### Implementation Detail

## Test Result


## Lessons Learned
- Synchronization problems are disasters.
- Using a lock in appropriate position is significant.
- Design review meeting is quite important for progressing the project.
- What a sequential creature human is!
- Let's figure out what is in scheduler's head at next project.
