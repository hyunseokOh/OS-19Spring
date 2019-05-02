#include "sched.h"

#include <linux/slab.h>
#include <linux/irq_work.h>

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *container) {
  wrr_rq->rq = container;
  wrr_rq->weight_sum = 0;
  wrr_rq->wrr_nr_running = 0;
}

const struct sched_class wrr_sched_class = {
  .next   = &fair_sched_class,
};
