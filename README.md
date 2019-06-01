# os-team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 4 - Geo-Tagged File System

## Implementation Details

### Modified/Implemented Files

1. `arch/arm64/include/asm/{unistd.h,unistd32.h}`, `include/linux/syscalls.h`
    - syscall related (easy now :smile:)
2. `kernel/Makefile`
    - add `kfloat.o, gps.o`
3. `include/linux/kfloat.h`
    - floating point arithmetic header
4. `kernel/kfloat.c`
    - floating point arithmetic implementation
5. `include/linux/gps.h`
    - define `struct gps_location`
    - add some gps related inline functions
6. `kernel/gps.c`
    - syscall implementation
    - `can_access` function implementation which check two `struct gps_location` are closer than accuracy boundary
7. `include/linux/fs.h`
    - add `set_gps_location` and `get_gps_location` to `struct inode_operations`
8. `fs/ext2/inode.c`
    - add `ext2_set_gps_location` and `ext2_get_gps_location`
    - add `ext2_permission` which calls `can_access` when try to read file
9. `fs/ext2/ialloc.`
    - call `ext2_set_gps_location` in `ext2_new_inode` (ext2 inode create)
10. `fs/ext2/{namei.c,file.c,symlink.c}`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_dir_inode_operations`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_file_inode_operations`
    - register `ext2_permission` to `ext2_file_inode_operations`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_symlink_inode_operations`
    - we do not add `ext2_permission` to `ext2_dir_inode_operations` and `ext2_symlink_inode_operations`
        - gps location is tagged to `dir` and `symlink` but we can always read those files regardless of current location
11. some other files in `fs` and `fs/ext2`
    - when `inode`'s `mtime` is updated to `current_time`, call `set_gps_location` if `inode` is geo-tagged
    - we filtered target files via `$ grep -iRl "mtime =" fs/`
    - commant like `$ cat ${filename}` cannot update gps location since it changes just `atime`


### Floating Point Arithmetic

Detailed implementations are in [include/linux/kfloat.h](include/linux/kfloat.h) and [kernel/kfloat.c](kernel/kfloat.c)

#### Floating Point Representation

We defined `kfloat` type in `include/linux/kfloat.h` as 
```
typedef struct {
  long long value;
  int pos;
} kfloat;
```
Member `value` stores both integer and fractional parts.  
Member `pos` denotes decimal point's position from the rightmost of the `value`.  
For example, if we want to represent `10.3434` using `kfloat`, it becomes `kfloat value = { 103434, 4 };`  
Since the 64bit `long long` type can store number from `-9223372036854775808` to `9223372036854775807`, we can store **18** decimal points in maximum (when integer part is `0`)

#### Negation

`kfloat` negation works almost same as normal negation.  
It just check whether current `value` is `LLONG_MIN`.  
If `value` is `LLONG_MIN`, we cannot negate value since `LLONG_MAX` is `9223372036854775807`, not `9223372036854775808`. 
In that case, we just negate `value` into `LLONG_MAX`.

#### Addition / Subtraction

Let's think about `f1 + f2 = 10.3434 + 987.2` case.  
`kfloat` representation of each number is `f1 = { 103434, 4 } and f2 = { 9872, 1 }` respectively.  
To add (or subtract), we must align decimal point in the same position.  
Thus we zero padded `pow(10, MAX(f1.pos, f2.pos) - MIN(f1.pos, f2.pos))`, in our case is `power(10, 3)`, to `f2.value` which has `MIN POS`.  
If overflow occur when zero padding, we throw away the least significant decimal point from `MAX POS` value (or both).  
Now the addition becomes `103434 + 9872 * 1000 = 103434 + 9872000 = 9975434` with `pos = 4`, which represents `997.5434`.  
While adding two values, it also checks whether overflow (or underflow) occurs and throw away the least sgnificant number from both values.  
Subtraction is same as addition (`f1 - f2 = f1 + (-f2)`).  
We just negate value and add.

#### Multiplication

We just multiplicate `value = value1 * value2` and set `pos` as `pos = pos1 + pos2`.  
When multiply two values, we also check whether overflow (underflow) of multiplication occurs, and throw away the least significant decimal point if overflow occurs.  

#### Misc
- key idea of our implementation is we always check overflow (underflow) and takes the most accurate representation as possible
- we defined some useful constants for calculating distance
- at each computation (add/sub/multiply), we call `truncate(kfloat *f)` which removes redundant `0`s (when the computation result is `{ 305540, 4 } = 30.5540`, we can remove the last `0` since `30.5540 == 30.554` and decrement `pos`)

### GPS Location Based Access Permission

#### Calculate Distance

See [distance.pdf](distance.pdf)

#### EXT2 File Permission

As we mentioned, we implemented `ext2_permission` and registered it to `ext2_file_inode_operations`.  
`ext2_permission` calls `generic_permission`.  
However, when permission mode is `MAY_READ`, it calculates distance between current gps location and tagged gps location and return `-EACCES` when we cannot access file's gps location

## Test Result

## Lessons Learned
