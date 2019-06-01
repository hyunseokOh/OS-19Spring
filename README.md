# os-team8
![final5](https://user-images.githubusercontent.com/25524539/54601604-c7cfd080-4a82-11e9-81fd-bd870b4ff151.png)
- Taebum Kim
- Hyunseok Oh
- Seonghoon Seo

---
# Project 4 - Geo-Tagged File System

## Implementation Details

### Modified/Implemented Files

### Floating Point Arithmetic

Detailed implementations are in `include/linux/kfloat.h` and `kernel/kfloat.c`

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

Let's think about `10.3434 + 987.2` case.  
`kfloat` representation of each number is `{ 103434, 4 } and { 9872, 1 }` respectively.  
To add (or subtract), we must align decimal point in the same position.  
Thus we zero padded `1e+(MAX_POS - MIN_POS)`, in our case is `1e+(4 - 1) = 1e+3 = 1000`, to value which has `MIN_POS`.  
If overflow occur when zero padding, we throw away the least significant decimal point from `MAX_POS` value (or both).  
Now the addition becomes `103434 + 9872 * 1000 = 103434 + 9872000 = 9975434` with `pos = 4`, which represents `997.5434`.  
While adding two values, it also checks whether overflow (or underflow) occurs and throw away the least sgnificant number from both values.  
Subtraction is same as addition (`f1 - f2 = f1 + (-f2)`).  
We just negate value and add.

#### Multiplication

We just multiplicate two `value = value1 * value2` and set `pos` as `pos = pos1 + pos2`.  
When multiply two values, we also check whether overflow (underflow) of multiplication occurs, and handle it appropriately.

#### Misc

- We defined some usefule constants for calculating distance
- At each computation (add/sub/multiply), we call `truncate(kfloat *f)` which removes redundant `0`s (when the computation result is `{ 305540, 4 } = 30.5540`, we can remove the last `0` and decrement `pos`

### GPS Location Based Access Permission

## Test Result
