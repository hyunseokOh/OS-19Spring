#include "sched.h"

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/irq_work.h>

static inline struct wrr_rq *get_min_weight_wrr_rq(void) {
  /*
   * get wrr_rq whose weight sum is min
   */
  struct rq *rq;
  struct wrr_rq *wrr_rq;
  struct wrr_rq *result = NULL;
  int min_weight = INT_MAX;
  int i;
  for_each_possible_cpu(i) {
    if (i == FORBIDDEN_WRR_QUEUE) continue;
    rq = cpu_rq(i);
    wrr_rq = &rq->wrr;
    if (wrr_rq->weight_sum < min_weight) {
      result = wrr_rq;
    }
  }
  return result;
}

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *container) {
  INIT_LIST_HEAD(&wrr_rq->head);
  wrr_rq->rq = container;
  wrr_rq->weight_sum = 0;
  wrr_rq->wrr_nr_running = 0;
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq) {
  return wrr_rq->rq;
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
  wrr_rq->wrr_nr_running++;
  wrr_rq->weight_sum += wrr_se->weight;
#ifdef CONFIG_SCHED_DEBUG
  /* when debugging, print queue info after insert */
#endif
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq *wrr_rq = get_min_weight_wrr_rq();
  rq = rq_of_wrr_rq(wrr_rq);
  enqueue_wrr_entity(wrr_rq, wrr_se);
  add_nr_running(rq, 1);
}

static void dequeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se) {
  list_del(&wrr_se->wrr_node);
  wrr_rq->wrr_nr_running--;
  wrr_rq->weight_sum -= wrr_se->weight;
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;
  struct wrr_rq* wrr_rq = wrr_rq_of_wrr_se(wrr_se);
  rq = rq_of_wrr_rq(wrr_rq);
  dequeue_wrr_entity(wrr_rq, wrr_se);
  sub_nr_running(rq, 1);
}

static void yield_task_wrr(struct rq *rq) {
}

static bool yield_to_task_wrr(struct rq *rq, struct task_struct *p, bool preempt) {
  return true;
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags) {

}

static struct task_struct *pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
  return NULL;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p) {
}

#ifdef CONFIG_SMP
static int select_task_rq_wrr(struct task_struct *p, int task_cpu, int sd_flag, int flags) {
  return 0;
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
}

static void task_fork_wrr(struct task_struct *p) {
}

static void task_dead_wrr(struct task_struct *p) {
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
  return 0;
}
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
  .get_rr_interval = get_rr_interval_wrr,
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
