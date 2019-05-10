#include "sched.h"

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/irq_work.h>

DEFINE_MUTEX(load_lock); // need to grab lock when updating time
unsigned long time_balance; // global variable to keep track of time
// TODO (SH): if necessary, find an appropriate place for initialization

// actual load-balancing function
static void load_balance_wrr(void) {

  struct rq *rq;
  struct rq *rq_mix;
  struct rq *rq_max;
  struct wrr_rq *wrr_rq;
  struct task_struct *p;

  int load[NR_CPUS - 1];
  int cpu;
  int exist_valid_cpu = 0;
  int min_cpu=-1;
  int max_cpu=-1; // initialize to -1


  /* TODO (SH): Find proper plce to plce either of the two functions + corresponding enable functions */
  // local_irq_disable();
  // preempt_disable(); // local_irq_disable() covers preemption

  rcu_read_lock();
  // TODO (SH): Verify that this routine is safe even with offine cpus
  for_each_online_cpu(cpu) {
    if (cpu == FORBIDDEN_WRR_QUEUE) {
      continue;
    }

    // set valid flag to 1
    if(exist_valid_cpu==0) {
      ++exist_valid_cpu;
      min_cpu = cpu;
      max_cpu = cpu;
    }

    rq = cpu_rq(cpu);
    wrr_rq = &rq->wrr;

    load[cpu] = wrr_rq->weight_sum;

    // comparison is valid if there is at least one valid cpu
    if(exist_valid_cpu){
      if(load[min_cpu] > load[cpu]) {
        min_cpu = cpu;
      }
     if(load_max_cpu] < load[cpu]) {
        max_cpu = cpu;
     }
    }
  }
  rcu_read_unlock();

  // No CPU is available for load balancing
  if(!exist_valid_cpu) {
    return;
  }

  rq_min = cpu_rq(min_cpu);
  rq_max = cpu_rq(max_cpu);

  // Only One CPU is available - no need for load balancing
  if (rq_min == rq_max) {
    return;
  }

  else {
    double_rq_lock(rq_min, rq_max);

    // loop over wrr_rq of rq_max to check to find if a task can be migrated
    struct list_head *traverse;
    struct list_head *tmp;
    struct sched_wrr_entity *wrr_se;
    struct task_struct *p;
    struct task_struct *task_to_be_migrated = NULL;
    int weight_of_task_to_be_migrated = 0;


    // TODO (SH): check exact syntax of macros
    list_for_each_safe(traverse, tmp, &rq_max->wrr.head) {
      wrr_se = container_of(traverse, struct sched_wrr_entity, wrr_node);
      p = container_of(wrr_se, struct task_struct, wrr);

      /* Migration Condition:
       * 1. the task should not be runnning
       * 2. CPU mask check
       * 3. weight condition check
       */

      // 1. task should not be currently running
      if (rq_max->curr == p) {
        double_rq_unlock(rq_min, rq_max);
        return;
      }

      // 2. CPU mask
      if (cpumask_test_cpu(rq_min->cpu, tsk_cpus_allowed(p)) == 0) {
        double_rq_unlock(rq_min, rq_max);
        return;
      }

      // 3. weight condition
      if (rq_max->wrr.weight_sum - wrr_se.weight < rq_min->wrr.weight_sum +  wrr_se.weight) {
        double_rq_unlock(rq_min, rq_max);
        return;
      }

      // Migration Condition Satisfied: Compare with current candidate
      if (task_to_be_migrated==NULL){
        task_to_be_migrated = p;
        weight_of_task_to_be_migrated = wrr_se->weight;
      }
      else{
        if (weight_of_task_to_be_migrated < wrr_se->weight) {
          task_to_be_migrated = p;
          weight_of_task_to_be_migrated = wrr_se->weight;
        }
      }

    }

    if (task_to_be_migrated == NULL) {
      double_rq_unlock(rq_min, rq_max);
      return;
    }
    // Finally, time for migration
    else {
      deactivate_task(rq_max, task_to_be_migrated, 0);
      set_task_cpu(task_to_be_migrated, rq_min->cpu);
      activate_task(rq_min, task_to_be_migrated, 0);
      double_rq_unlock(rq_min, rq_max);
    }

  }

}

// checks whether 2000ms has passed and calls load_balance if time has expired
static void trigger_load_balance_wrr(void) {
  mutex_lock(&load_lock);
  unsigned long curr_time = time_balance; // Does jiffies get updated even when the code is being executed?

  if (jiffies < time_balance + LOAD_BALANCE_INTERVAL) {
    mutex_unlock(&load_lock);
    return;
  }
  else {
    time_balance = curr_time;
    mutex_unlock(&load_lock);
    load_balance_wrr(); // is it okay to call it as separate function?
  }
}

int get_target_cpu(int flag, int rcu_lock) {
  struct rq *rq;
  struct wrr_rq *wrr_rq;
  int cpu = 0, cpu_iter;
  int weight = (flag == WRR_GET_MIN) ? INT_MAX : 0;

  if (rcu_lock) rcu_read_lock();
  for_each_online_cpu(cpu_iter) {
    if (cpu_iter == FORBIDDEN_WRR_QUEUE) continue;
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

static inline struct wrr_rq *wrr_rq_of_wrr_se(struct sched_wrr_entity *wrr_se) {
  return wrr_se->wrr_rq;
}

static void enqueue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) {
  struct list_head *queue = &wrr_rq->head;
  list_add_tail(&wrr_se->wrr_node, queue);
  wrr_se->wrr_rq = wrr_rq;
  wrr_se->on_rq = 1;

  wrr_rq->wrr_nr_running++;
  wrr_rq->weight_sum += wrr_se->weight;
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq *wrr_rq = &rq->wrr;
  struct rq *min_rq = rq_of_wrr_rq(wrr_rq);

  int cpu;
  for_each_cpu(cpu, &p->cpus_allowed) {
    printk("GONGLE; allowed cpu = %d\n", cpu);
  }

#ifdef CONFIG_GONGLE_DEBUG
  printk("Before enqueue, weight_sum = %d, running = %d cpu = %d\n", wrr_rq->weight_sum, wrr_rq->wrr_nr_running, cpu_of(min_rq));
#endif
  enqueue_wrr_entity(wrr_rq, wrr_se);
  add_nr_running(min_rq, 1);
#ifdef CONFIG_GONGLE_DEBUG
  printk("After enqueue, weight_sum = %d, running = %d cpu = %d\n", wrr_rq->weight_sum, wrr_rq->wrr_nr_running, cpu_of(min_rq));
#endif
}

static void dequeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) {
  list_del(&wrr_se->wrr_node);
  wrr_se->on_rq = 0;

  wrr_rq->wrr_nr_running--;
  wrr_rq->weight_sum -= wrr_se->weight;
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq* wrr_rq = wrr_rq_of_wrr_se(wrr_se);
  struct rq* min_rq = rq_of_wrr_rq(wrr_rq);

#ifdef CONFIG_GONGLE_DEBUG
  printk("Before dequeue, weight_sum = %d, running = %d, cpu = %d\n", wrr_rq->weight_sum, wrr_rq->wrr_nr_running, cpu_of(min_rq));
#endif
  dequeue_wrr_entity(wrr_rq, wrr_se);
  sub_nr_running(min_rq, 1);
#ifdef CONFIG_GONGLE_DEBUG
  printk("After dequeue, weight_sum = %d, running = %d, cpu = %d\n", wrr_rq->weight_sum, wrr_rq->wrr_nr_running, cpu_of(min_rq));
#endif
}

static void requeue_task_wrr(struct rq *rq, int flags) {
  /* set first element to become tail */
  struct wrr_rq *wrr_rq = &rq->wrr;
  list_rotate_left(&wrr_rq->head);
}

static void yield_task_wrr(struct rq *rq) {
  requeue_task_wrr(rq, WRR_REQUEUE_YIELD);
}

static bool yield_to_task_wrr(struct rq *rq, struct task_struct *p, bool preempt) {
  return true;
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {

}

static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
  /*
   * FIXME (taebum) do we need to use prev and rf?
   */
  struct wrr_rq *wrr_rq = &rq->wrr;
  struct sched_wrr_entity *wrr_se;
  struct list_head *queue = &wrr_rq->head;
  struct task_struct *p;
  if (list_empty(queue)) {
    return NULL;
  } else {
    wrr_se = list_first_entry(queue, struct sched_wrr_entity, wrr_node);
    p = container_of(wrr_se, struct task_struct, wrr);
    return p;
  }
}


static void put_prev_task_wrr(struct rq *rq, struct task_struct *p) {
}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
  int cpu = get_target_cpu(WRR_GET_MIN, 1);
#ifdef CONFIG_GONGLE_DEBUG
  printk("GONGLE: select task_rq_wrr called, assign to %d\n", cpu);
#endif
  return cpu;
}
static void migrate_task_rq_wrr(struct task_struct *p) {
}
static void task_woken_wrr(struct rq *this_rq, struct task_struct *task) {
}
static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *newmask) {
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

  // Decrement time slice of sched_wrr_entity
  if(--wrr_se->time_slice) {
    return; // time slice still remains
  }

  /*
   * restore timeslice
   * modified weight will change timeslice at this moment
   */
  wrr_se->time_slice = WRR_TIMESLICE(wrr_se->weight);

  // if there are more than 1 sched_wrr entities in the wrr_rq
  if (wrr_rq->wrr_nr_running > 1) {
    
    // move wrr_se to end of queue and reschedule it
    list_move_tail(&wrr_se->wrr_node, &rq->wrr.head);
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
  if (task == NULL) {
    return;
  }
  struct sched_wrr_entity *wrr_se = &task->wrr;
  wrr_se->weight = WRR_DEFAULT_WEIGHT;
  wrr_se->time_slice = WRR_TIMESLICE(WRR_DEFAULT_WEIGHT);
}
static void prio_changed_wrr(struct rq *this_rq, struct task_struct *task, int oldprio) {
}
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
  return 0;
}
static void update_curr_wrr(struct rq *rq) {
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
  printk("GONGLE: [weight = %u, time_slice = %u, on_rq = %u\n",
      wrr_se->weight, wrr_se->time_slice, (unsigned int) wrr_se->on_rq);
}

void print_wrr_stats(struct seq_file *m, int cpu) {
  struct wrr_rq *wrr_rq = &cpu_rq(cpu)->wrr;
  rcu_read_lock();
  print_wrr_rq(m, cpu, wrr_rq);
  rcu_read_unlock();
}

#endif
