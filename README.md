# os-team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 4 - Geo-Tagged File System

## Table of Contents
- [Implementation Details](#implementation-details)
    - [Floating Point Arithmetic](#floating-point-arithmetic)
    - [GPS Location Based Access Permission](#gps-location-based-access-permission)
- [Test](#test)
    - [Prepare Test](#prepare-test)
    - [Floating Point Accuracy](#floating-point-accuracy)
    - [Rename Behaviour](#rename-behaviour)
    - [file_loc Test](#file_loc-test)
- [Lessons Learned](#lessons-learned)

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
8. `fs/ext2/ext2.h`
    - add GPS-related fields to `struct ext2_inode` and `struct ext2_inode_info`
9. `fs/ext2/inode.c`
    - add `ext2_set_gps_location` and `ext2_get_gps_location`
    - add `ext2_permission` which calls `can_access` when try to read file
    - add endian conversion for GPS-related fields in `ext2_iget()` and `__ext2_write_inode()`
10. `fs/ext2/ialloc.c`
    - call `ext2_set_gps_location` in `ext2_new_inode` (ext2 inode create)
11. `fs/ext2/{namei.c,file.c,symlink.c}`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_dir_inode_operations`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_file_inode_operations`
    - register `ext2_permission` to `ext2_file_inode_operations`
    - register `ext2_set_gps_location` and `ext2_get_gps_location` to `ext2_symlink_inode_operations`
    - we do not add `ext2_permission` to `ext2_dir_inode_operations` and `ext2_symlink_inode_operations`
        - gps location is tagged to `dir` and `symlink` but we can always read those files regardless of current location
12. some other files in `fs` and `fs/ext2`
    - when `inode`'s `mtime` is updated to `current_time`, call `set_gps_location` if `inode` is geo-tagged
    - we filtered target files via `$ grep -iRl "mtime =" fs/`
    - command like `$ cat ${filename}` cannot update gps location since it changes just `atime`


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
Member `value` stores both integer and fractional parts together.  
Member `pos` denotes decimal point's position from the rightmost of the `value`.  
For example, if we want to represent `10.3434` using `kfloat`, it becomes `kfloat value = { 103434, 4 };`  
Since the 64bit `long long` type can store number from `LLONG_MIN = -9223372036854775808` to `LLONG_MAX = 9223372036854775807`, we can store **18** decimal points in maximum (when integer part is `0`)

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
If overflow occur when zero padding, we throw away the least significant decimal point from `f1.value`.  
Now the addition becomes `103434 + 9872 * 1000 = 103434 + 9872000 = 9975434` with `pos = 4`, which represents `997.5434`.  
While adding two values, it also checks whether overflow (or underflow) occurs and throw away the least sgnificant number from both values if it occurs.  
Subtraction is same as addition (`f1 - f2 = f1 + (-f2)`).  
We just negate value and add.

#### Multiplication

We just multiplicate `value = value1 * value2` and set `pos` as `pos = pos1 + pos2`.  
When multiply two values, we also check whether overflow (underflow) of multiplication occurs, and throw away the least significant decimal point if overflow occurs.  
We did not implement division since it is inverse multiplication and we did not need arbitrary division computation.  
However, since we needed some constant division, we pre-defined some constants like `factorials[20]`, which holds `1/0!, 1/1!, 1/2!, ... 1/18!, 1/19!` with 18 decimal points (maximum precision of our `kfloat` type).  

#### Misc
- key idea of our implementation is we always check overflow (underflow) and takes the most accurate precision as possible
- we defined some useful constants for calculating distance (`factorials`, `deg2radian`, `rad2degree`...)
- at each computation (add/sub/multiply), we call `truncate(kfloat *f)` which removes redundant `0`s (when the computation result is `{ 305540, 4 } = 30.5540`, we can remove the last `0` since `30.5540 == 30.554` and decrement `pos`)

### GPS Location Based Access Permission

#### Calculate Distance

See [distance.pdf](distance.pdf)

#### EXT2 File Permission

As we mentioned, we implemented `ext2_permission` and registered it to `ext2_file_inode_operations`.  
`ext2_permission` calls `generic_permission`.  
However, when permission mode is `MAY_READ`, it calculates distance between current gps location and tagged gps location and return `-EACCES` when we cannot access file's gps location.  
We did not register `ext2_permission` to `dir` or `symlink`.

## Test

### Prepare Test

You can simply type
```
$ cd test
$ make
```
then two binary files, `file_loc` and `gpsupdate` are created in `test`

### Floating Point Accuracy

To check our floating point representation works correct, we duplicated them in user-side and compared with math library of C (`<math.h>`).  
The cosine value of angle between (`f` value which we mentioned in [distance.pdf](distance.pdf)) `bld301` and `lacucina` is
```
Our approximation = 0.999999998565644588
C Library         = 0.999999999406627205
Relative Error    = 8.409826173777211e-10
```
The cosine value of Seoul City Hall and Tokyo Metropolitan Government Building is
```
Our approximation = 0.983679292725880061
C Library         = 0.983679293768168872
Relative Error    = 1.0595819363265917e-09
```
The cosine value of angle between `bld301` and `mit` (MIT library) is
```
Our approximation = -0.151112455832647458 (about 98.69 degree)
C Library         = -0.148085108868682758 (about 98.52 degree)
Relative Error    =  0.020443304372960786 (about 2%)
```
Although magnitude of the angle between `bld301` and `mit` is about 98.52 degree, our approximation calculates it to 98.69 degree.  
We can confirm that its quite accurate even for far distance.


### Rename Behaviour

One of the interesting behaviour is `rename` file does not update `mtime`.  
When we look `ext2_rename` in `fs/ext2/dir.c`, we can check that it only updates `ctime`.  
The example of test is
```
root:~/proj4> echo "file" > file
root:~/proj4> stat file
  File: `file'
  Size: 5         	Blocks: 2          IO Block: 1024   regular file
Device: 700h/1792d	Inode: 21          Links: 1
Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
Access: 1970-01-01 00:22:15.000000000 +0000
Modify: 1970-01-01 00:22:15.000000000 +0000
Change: 1970-01-01 00:22:15.000000000 +0000
root:~/proj4> ../file_loc file 
File path = file
Latitude = 0.000000, Longitude = 0.000000
Accuracy = 0 [m]
Google Link = https://maps.google.com/?q=0.000000,0.000000
root:~/proj4> ../gpsupdate 0 0 0 0 100
root:~/proj4> mv file refile 
root:~/proj4> ../file_loc refile 
File path = refile
Latitude = 0.000000, Longitude = 0.000000
Accuracy = 0 [m]
Google Link = https://maps.google.com/?q=0.000000,0.000000
root:~/proj4> stat refile
  File: `refile'
  Size: 5         	Blocks: 2          IO Block: 1024   regular file
Device: 700h/1792d	Inode: 21          Links: 1
Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
Access: 1970-01-01 00:22:15.000000000 +0000
Modify: 1970-01-01 00:22:15.000000000 +0000
Change: 1970-01-01 00:22:42.000000000 +0000
```
We can check that only Change time updated.

### `file_loc` Test

We've created 1 directory and 3 files

| Filename | Location                                      |
|------------|-----------------------------------------------|
| `google`   | Google mountain view head quarter (directory) |
| `bld301`   | Building 301 in SNU                           |
| `lacucina` | Lacucina wedding hall in SNU                  |
| `mit`      | Library of MIT                                |

Outputs are
```
root:~/proj4> ../file_loc google/
File path = google/
Latitude = 37.422059, Longitude = -122.084057
Accuracy = 50 [m]
Google Link = https://maps.google.com/?q=37.422059,-122.084057

root:~/proj4> ../file_loc bld301 
File path = bld301
Latitude = 37.450048, Longitude = 126.952503
Accuracy = 10 [m]
Google Link = https://maps.google.com/?q=37.450048,126.952503

root:~/proj4> ../file_loc lacucina 
File path = lacucina
Latitude = 37.448526, Longitude = 126.950920
Accuracy = 50 [m]
Google Link = https://maps.google.com/?q=37.448526,126.950920

root:~/proj4> ../file_loc mit
File path = mit
Latitude = 42.359034, Longitude = -71.089137
Accuracy = 50 [m]
Google Link = https://maps.google.com/?q=42.359034,-71.089137
```

## Lessons Learned

- Implementing floating point using integer is fun :)
- Learned design pattern through proj3, proj4 (function pointer template)
- Project + Project + Project + Project = June
- Number of times we flashed the sd card = ???
- We now have a magic compass to navigate through a flood of kernel codes!
