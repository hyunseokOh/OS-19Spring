#include "sched.h"

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/irq_work.h>


#ifdef CONFIG_WRR_BALANCE_TEST
DEFINE_MUTEX(coin_lock);
int coin = 0;
#endif

/* cannot use mutex_lock in scheduler related functions */
DEFINE_SPINLOCK(load_balance_lock); // need to grab lock when updating time
unsigned long old_jiffies = 0;

// actual load-balancing method
static void load_balance_wrr(void) {
  struct rq *rq;
  struct rq *rq_min;
  struct rq *rq_max;
  struct wrr_rq *wrr_rq;
  unsigned long flags;

  int load[NR_CPUS];
  int cpu;

  int min_cpu = -1;
  int max_cpu = -1;

  /* for migration */
  struct list_head *traverse;
  struct sched_wrr_entity *wrr_se;
  struct task_struct *p;
  struct task_struct *migrate_task = NULL;
  int migrate_task_weight = 0;

  rcu_read_lock();
  for_each_online_cpu(cpu) {
    if (cpu == FORBIDDEN_WRR_QUEUE) {
      continue;
    }

    /* first touch */
    if (min_cpu == -1) min_cpu = cpu;
    if (max_cpu == -1) max_cpu = cpu;

    rq = cpu_rq(cpu);
    wrr_rq = &rq->wrr;

    load[cpu] = wrr_rq->weight_sum;

    // comparison is valid if there is at least one valid cpu
    if (min_cpu != -1 && load[min_cpu] > load[cpu]) {
      min_cpu = cpu;
    }

    if (max_cpu != -1 && load[max_cpu] < load[cpu]) {
      max_cpu = cpu;
    }
  }
  rcu_read_unlock();

  // Only One CPU is available - no need for load balancing
  if (min_cpu == max_cpu) {
    return;
  }

  // No CPU is available for load balancing
  if(min_cpu == -1 || max_cpu == -1) {
    return;
  }

  /* get rq */
  rq_min = cpu_rq(min_cpu);
  rq_max = cpu_rq(max_cpu);

  // referenced from yield_to method in core.c
  // saves interrupt delivery after saving current interrupt flags
  local_irq_save(flags);
  double_rq_lock(rq_min, rq_max);

  // loop over wrr_rq of rq_max to check to find if a task can be migrated
  list_for_each(traverse, &rq_max->wrr.head) {
    wrr_se = container_of(traverse, struct sched_wrr_entity, wrr_node);
    p = container_of(wrr_se, struct task_struct, wrr);

    /* 
     * Migration Condition:
     * 1. the task should not be runnning
     * 2. CPU mask check
     * 3. weight condition check
     */

    // 1. task should not be currently running
    if (rq_max->curr == p) {
      continue;
    }

    // 2. CPU mask
    if (cpumask_test_cpu(min_cpu, &(p->cpus_allowed)) == 0) {
      continue;
    }

    // 3. weight condition: migration should not cause the weight of RQ_MIN to become "bigger than or equal to" RQ_MAX
    if (rq_max->wrr.weight_sum - wrr_se->weight <= rq_min->wrr.weight_sum +  wrr_se->weight) {
      continue;
    }

    // Migration Condition Satisfied: Compare with current candidate
    if (migrate_task == NULL){
      /* first searched */
      migrate_task = p;
      migrate_task_weight = wrr_se->weight;
    } else{
      /* already have one */
      if (migrate_task_weight < wrr_se->weight) {
        /* we can do tighter load balance */
        migrate_task = p;
        migrate_task_weight = wrr_se->weight;
      }
    }
  }

  if (migrate_task == NULL) {
    // End since there is no valid task to be migrated
    double_rq_unlock(rq_min, rq_max);
    local_irq_restore(flags);
    return;
  }

  // Finally, time for migration
  deactivate_task(rq_max, migrate_task, 0);
  set_task_cpu(migrate_task, min_cpu);
  activate_task(rq_min, migrate_task, 0);

  double_rq_unlock(rq_min, rq_max);
  local_irq_restore(flags);
  return;
}

static inline int on_null_domain(struct rq *rq)
{
  /* got from kernel/sched/fair.c */
	return unlikely(!rcu_dereference_sched(rq->sd));
}

// checks whether 2000ms has passed and calls load_balance if time has expired
void trigger_load_balance_wrr(struct rq *rq) {
  /*
   * jiffies overflow ref:
   *  https://stackoverflow.com/questions/8206762/how-does-linux-handle-overflow-in-jiffies
   */
  if (unlikely(on_null_domain(rq))) return;

  spin_lock(&load_balance_lock);
  if (time_before(jiffies, old_jiffies + LOAD_BALANCE_INTERVAL)) {
    spin_unlock(&load_balance_lock);
    return;
  } else {
    old_jiffies = jiffies;
    spin_unlock(&load_balance_lock);
    load_balance_wrr();
  }
}

int get_target_cpu(int flag, int rcu_lock, struct task_struct *p) {
  struct rq *rq;
  struct wrr_rq *wrr_rq;
  struct cpumask mask;
  int cpu, cpu_iter;
  int weight = (flag == WRR_GET_MIN) ? INT_MAX : 0;

  if (p != NULL) {
    cpumask_copy(&mask, &p->cpus_allowed);
    cpu = task_cpu(p); /* keep current cpu */
    if (cpu == FORBIDDEN_WRR_QUEUE) cpu = -1;
  } else {
    cpu = -1; /* cannot assign */
  }

  if (rcu_lock) rcu_read_lock();
  for_each_online_cpu(cpu_iter) {
    if (cpu_iter == FORBIDDEN_WRR_QUEUE) continue;
    if (p != NULL && !cpumask_test_cpu(cpu_iter, &mask)) continue;
    rq = cpu_rq(cpu_iter);
    wrr_rq = &rq->wrr;
    if (flag == WRR_GET_MIN) {
      if (wrr_rq->weight_sum < weight) {
        cpu = cpu_of(rq);
        weight = wrr_rq->weight_sum;
      }
    } else if (flag == WRR_GET_MAX) {
      if (wrr_rq->weight_sum > weight) {
        cpu = cpu_of(rq);
        weight = wrr_rq->weight_sum;
      }
    }
  }
  if (rcu_lock) rcu_read_unlock();
  return cpu;
}

void init_wrr_rq(struct wrr_rq *wrr_rq) {
  INIT_LIST_HEAD(&wrr_rq->head);
  wrr_rq->weight_sum = 0;
  wrr_rq->wrr_nr_running = 0;
}

static inline void requeue_task_wrr(struct rq *rq) {
  /* set first element to become tail */
  struct wrr_rq *wrr_rq = &rq->wrr;
  list_rotate_left(&wrr_rq->head);
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se) {
  return wrr_se->on_rq;
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq) {
  return container_of(wrr_rq, struct rq, wrr);
}

static inline struct rq *rq_of_wrr_se(struct sched_wrr_entity *wrr_se) {
  struct wrr_rq *wrr_rq = wrr_se->wrr_rq;

  return rq_of_wrr_rq(wrr_rq);
}

static void update_curr_wrr(struct rq *rq) {
}

static inline struct wrr_rq *wrr_rq_of_wrr_se(struct sched_wrr_entity *wrr_se) {
  return wrr_se->wrr_rq;
}

static void enqueue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) {
  struct list_head *queue = &wrr_rq->head;
  struct rq *rq = rq_of_wrr_rq(wrr_rq);

  list_add_tail(&wrr_se->wrr_node, queue);
  wrr_se->wrr_rq = wrr_rq;
  wrr_se->on_rq = 1;

  wrr_rq->wrr_nr_running++;
  wrr_rq->weight_sum += wrr_se->weight;

  add_nr_running(rq, 1);
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq *wrr_rq = &rq->wrr;

  if (wrr_se->on_rq == 1) return;
  
  BUG_ON(cpu_of(rq) == FORBIDDEN_WRR_QUEUE);

  enqueue_wrr_entity(wrr_rq, wrr_se);
}

static void dequeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) {
  struct rq *rq = rq_of_wrr_rq(wrr_rq);

  list_del(&wrr_se->wrr_node);
  wrr_se->on_rq = 0;

  wrr_rq->wrr_nr_running--;
  wrr_rq->weight_sum -= wrr_se->weight;

  sub_nr_running(rq, 1);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq* wrr_rq = wrr_rq_of_wrr_se(wrr_se);

  if (wrr_se->on_rq == 0) return;

  dequeue_wrr_entity(wrr_rq, wrr_se);
}

static void yield_task_wrr(struct rq *rq) {
  requeue_task_wrr(rq);
}

static bool yield_to_task_wrr(struct rq *rq, struct task_struct *p, bool preempt) {
  return true;
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {
}

static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
  struct wrr_rq *wrr_rq = &rq->wrr;
  struct sched_wrr_entity *wrr_se;
  struct list_head *queue = &wrr_rq->head;
  struct task_struct *p;

  if (list_empty(queue)) {
    return NULL;
  } else {
    put_prev_task(rq, prev);
    wrr_se = list_first_entry(queue, struct sched_wrr_entity, wrr_node);
    p = container_of(wrr_se, struct task_struct, wrr);
    return p;
  }
}


static void put_prev_task_wrr(struct rq *rq, struct task_struct *p) {
}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
#ifdef CONFIG_WRR_BALANCE_TEST
  /* just coin flip between cpu 0 and 1 (force assign) */
  int retval;
  mutex_lock(&coin_lock);
  retval = coin++ % 2;
  mutex_unlock(&coin_lock);
  return retval;
#else
  return get_target_cpu(WRR_GET_MIN, 1, p);
#endif
}
static void migrate_task_rq_wrr(struct task_struct *p) {
}
static void task_woken_wrr(struct rq *this_rq, struct task_struct *task) {
}
static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *new_mask) {
  set_cpus_allowed_common(p, new_mask);
}
static void rq_online_wrr(struct rq *rq) {
}
static void rq_offline_wrr(struct rq *rq) {
}
#endif

static void set_curr_task_wrr(struct rq *rq) {
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued) {
  /* Implementation mimics the flow of task_tick_rt */
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq *wrr_rq = wrr_se->wrr_rq;

  BUG_ON(wrr_rq != &rq->wrr);

  if (!wrr_policy(p->policy)) {
    return;
  }

  // Decrement time slice of sched_wrr_entity
  if (--wrr_se->time_slice) {
    return; // time slice still remains
  }

  /*
   * restore timeslice
   * modified weight will change timeslice at this moment
   */
  wrr_se->time_slice = WRR_TIMESLICE(wrr_se->weight);

  // if there are more than 1 sched_wrr entities in the wrr_rq
  if (wrr_se->wrr_node.next != &wrr_rq->head) {
    // move wrr_se to end of queue and reschedule it
    list_move_tail(&wrr_se->wrr_node, &wrr_rq->head);
    resched_curr(rq);
  }
}

static void task_fork_wrr(struct task_struct *p) {
}

static void task_dead_wrr(struct task_struct *p) {
}

static void switched_from_wrr(struct rq *this_rq, struct task_struct *task) {
}

static void switched_to_wrr(struct rq *this_rq, struct task_struct *task) {
  struct sched_wrr_entity *wrr_se = &task->wrr;

  /* just refill time slice */
  wrr_se->time_slice = WRR_TIMESLICE(WRR_DEFAULT_WEIGHT);
}

static void prio_changed_wrr(struct rq *this_rq, struct task_struct *task, int oldprio) {
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
  return 0;
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void task_change_group_wrr(struct task_struct *p, int type) {
}
#endif
const struct sched_class wrr_sched_class = {
  .next   = &fair_sched_class,
  .enqueue_task = enqueue_task_wrr,
  .dequeue_task = dequeue_task_wrr,
  .yield_task = yield_task_wrr,
  .yield_to_task = yield_to_task_wrr,
  .check_preempt_curr = check_preempt_curr_wrr,
  .pick_next_task = pick_next_task_wrr,
  .put_prev_task = put_prev_task_wrr,
#ifdef CONFIG_SMP
  .select_task_rq = select_task_rq_wrr,
  .migrate_task_rq = migrate_task_rq_wrr,
  .task_woken = task_woken_wrr,
  .set_cpus_allowed = set_cpus_allowed_wrr,
  .rq_online = rq_online_wrr,
  .rq_offline = rq_offline_wrr,
#endif
  .set_curr_task = set_curr_task_wrr,
  .task_tick = task_tick_wrr,
  .task_fork = task_fork_wrr,
  .task_dead = task_dead_wrr,
  .switched_from = switched_from_wrr,
  .switched_to = switched_to_wrr,
  .prio_changed = prio_changed_wrr,
  .get_rr_interval = get_rr_interval_wrr,
  .update_curr = update_curr_wrr,

#ifdef CONFIG_FAIR_GROUP_SCHED
  .task_change_group = task_change_group_wrr,
#endif
};


#ifdef CONFIG_SCHED_DEBUG
/* now comes debugging functions */

void printk_sched_wrr_entity(struct sched_wrr_entity *wrr_se) {
  printk("GONGLE: [weight = %u, time_slice = %u, on_rq = %u]\n",
      wrr_se->weight, wrr_se->time_slice, (unsigned int) wrr_se->on_rq);
}

void printk_wrr_rq(int cpu) {
  struct wrr_rq *wrr_rq = &cpu_rq(cpu)->wrr;
  struct list_head *head = &wrr_rq->head;
  struct list_head *traverse;
  struct sched_wrr_entity *wrr_se;

  printk("\nwrr_rq[%d]:\n", cpu);
  printk("  .wrr_nr_running = %u\n", wrr_rq->wrr_nr_running);
  printk("  .wrr_weight_sum = %u\n", wrr_rq->weight_sum);

  list_for_each(traverse, head) {
    wrr_se = container_of(traverse, struct sched_wrr_entity, wrr_node);
    printk_sched_wrr_entity(wrr_se);
  }
}

void printk_all_wrr_rq(void) {
  int cpu;

  rcu_read_lock();

  for_each_online_cpu(cpu) {
    if (cpu == FORBIDDEN_WRR_QUEUE) {
      continue;
    }
    printk_wrr_rq(cpu);
  }

  rcu_read_unlock();
}

void print_wrr_stats(struct seq_file *m, int cpu) {
  struct wrr_rq *wrr_rq = &cpu_rq(cpu)->wrr;
  rcu_read_lock();
  print_wrr_rq(m, cpu, wrr_rq);
  rcu_read_unlock();
}

#endif
