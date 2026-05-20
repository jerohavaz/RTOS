#include "prio_waitq.h"

static uint32_t highest_prio_from_bitmap(uint32_t bitmap) {
    for (int32_t p = (int32_t)OS_MAX_PRIORITIES - 1; p >= 0; p--) {
        if ((bitmap & (1u << (uint32_t)p)) != 0u) {
            return (uint32_t)p;
        }
    }

    return 0u;
}

void prio_waitq_init(prio_waitq_t *q, task_node_fn_t get_node) {
    q->bitmap = 0u;

    for (uint32_t i = 0u; i < OS_MAX_PRIORITIES; i++) {
        task_list_init(&q->prio[i], get_node);
    }
}

uint8_t prio_waitq_is_empty(const prio_waitq_t *q) {
    return q->bitmap == 0u;
}

void prio_waitq_push(prio_waitq_t *q, tcb_t *task) {
    uint32_t p = task->prio;

    task_list_push_back(&q->prio[p], task);
    q->bitmap |= (1u << p);
}

tcb_t *prio_waitq_pop_highest(prio_waitq_t *q) {
    if (q->bitmap == 0u) {
        return 0;
    }

    uint32_t p = highest_prio_from_bitmap(q->bitmap);

    tcb_t *task = task_list_pop_front(&q->prio[p]);

    if (task_list_is_empty(&q->prio[p])) {
        q->bitmap &= ~(1u << p);
    }

    return task;
}

void prio_waitq_remove(prio_waitq_t *q, tcb_t *task) {
    uint32_t p = task->prio;

    task_list_remove(&q->prio[p], task);

    if (task_list_is_empty(&q->prio[p])) {
        q->bitmap &= ~(1u << p);
    }
}