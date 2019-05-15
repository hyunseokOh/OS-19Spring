# How can we improve WRR scheduler

## Modify Load Balance Criterion

Current load balancing criterion does not allow min weight queue's weight and max weight queue's weight become equal.  
However, consider following case, if two tasks which have same weight are only entity in whole system and they are assigned to the first cpu.  
Current load balancing criterion does not distribute those tasks (move one of the task into other cpu) since weight becomes same.  
If we change criterion to allow **same weight** after load balance, we can make much more balanced task distribution. 

## Allow Kernel to Modify Weight of Task

In current implementation only users can modify task's weight via `sys_sched_set_weight`.  
Let's think about case that kernel has privilege to modify weight.  
For each task, kernel monitors total run time of task (cumulated time slice).  
We have certain threshold `k`, if total run time of the task exceeds `k` and if the task always consumes its own time slice (not resched before consuming its time slice), kernel increases weight of task (might be cpu-bound task).  
On the other hand, it the task always rescheduled before it consumes its time slice, we can treat it as I/O bound task.  
For such tasks, we can decrease it's weight based on some threshold.  
If we apply these conditions, we can distribute tasks, in various range of weight.  

## Manage Tasks in Red Black Tree

Current WRR scheduler has `O(n)` time complexity when it tries to do load balance since it traverses all tasks in max weight queue.  
If we manage tasks not only list, but also red black tree as weight as key value, we can reduce this time complexity.  
When we get min weight queue and max weight queue, we can calculate the maximum weight we can migrate from max weight queue to min weight queue.  
For example, assume that max weight queue has total weight as `20`, and min weight queue has total weight as `15`.  
Then the maximum weight we can migrate is `2` (total weight become `18` and `17` respectively).  
We can find target task in red black tree, as `2` as search key (not exact search, closest search).  
Then the load balance overhead becomes `O(log n)`.  
However, this approach raises other overhead.  
Current implementation allows enqueue task and dequeue task in `O(1)`.  
If we use red black tree implementation, every time when we enqueue or dequeue task, it takes `O(log n)` time complexity (tree insertion and deletion).  

## Use Multiple Queue

Alternative way to reduce load balance overhead is using multiple queues.  
Current WRR scheduler has a single queue per cpu, and all tasks are in same queue even they has different weight.  
Now, let's use 4 queues. Each queue covers separate range of weight, `1 - 5`, `6 - 10`, `11 - 15`, `16 - 20`.  
As we mentioned before, when we get min weight queue and max weight queue, we can calculate the maximum weight we can migrate from max weight queue to min weight queue.  
If the maximum weight of migrate-able task is `13`, now we can only lookup third queue which has tasks whose weights are `11 - 15`.  
This approach cannot reduce asymptotic running time of load balance, but it can reduce actual run time in real usage.  
Extremely, we can use 20 queues, each cover single weight from 1 to 20.
