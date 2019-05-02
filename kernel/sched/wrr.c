#include "sched.h"

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/irq_work.h>

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *container) {
  INIT_LIST_HEAD(&wrr_rq->wrr_rq_head);
  wrr_rq->rq = container;
  wrr_rq->weight_sum = 0;
  wrr_rq->wrr_nr_running = 0;
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq) {
  return wrr_rq->rq;
}

static inline struct rq *rq_of_wrr_se(struct sched_wrr_entity *wrr_se) {
  struct wrr_rq *wrr_rq = wrr_se->wrr_rq;

  return wrr_rq->rq;
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se) {
  return wrr_se->wrr_rq;
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags) {
  struct sched_wrr_entity *wrr_se = &p->wrr;

}

const struct sched_class wrr_sched_class = {
  .next   = &fair_sched_class,
  .enqueue_task = enqueue_task_wrr,
  .dequeue_task = dequeue_task_wrr,
};
