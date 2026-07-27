#include "prio_waitq.h"
#include "kernel_panic.h"
#include "kernel_task.h"
#include "task_list.h"
#include "tcb.h"
#include <stdbool.h>

#if (OS_MAX_PRIORITIES == 0u)
#error "OS_MAX_PRIORITIES must be greater than 0"
#endif

#if (OS_MAX_PRIORITIES > 32u)
#error "OS_MAX_PRIORITIES must be <= 32 because prio_waitq uses a uint32_t bitmap"
#endif

/**
 * @brief Return the highest set priority in a non-empty bitmap.
 *
 * Finds the index of the most significant set bit in the ready-priority
 * bitmap.
 *
 * @param bitmap Priority bitmap to inspect.
 *
 * @return Highest set priority index.
 *
 * @pre @p bitmap must not be 0.
 *
 * @note OS_MAX_PRIORITIES is checked at compile time because the ready queue
 *       stores priorities in a 32-bit bitmap.
 */
static uint32_t highest_prio_from_bitmap(uint32_t bitmap) {
    KERNEL_REQUIRE(bitmap != 0u);

    return 31u - (uint32_t)__builtin_clz(bitmap);
}

void prio_waitq_init(prio_waitq_t *q, task_node_fn_t get_node) {
    KERNEL_REQUIRE(q != 0);
    KERNEL_REQUIRE(get_node != 0);

    q->bitmap = 0u;

    for (uint32_t i = 0u; i < OS_MAX_PRIORITIES; i++) {
        task_list_init(&q->prio[i], get_node);
    }
}

bool prio_waitq_is_empty(const prio_waitq_t *q) {
    KERNEL_REQUIRE(q != 0);

    return (q->bitmap == 0u);
}

void prio_waitq_push(prio_waitq_t *q, kernel_task_t *task) {
    KERNEL_REQUIRE(q != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->tcb.u8TaskPrio < OS_MAX_PRIORITIES);

    uint32_t p = task->tcb.u8TaskPrio;

    task_list_push_back(&q->prio[p], task);
    q->bitmap |= (1u << p);
}

kernel_task_t *prio_waitq_pop_highest(prio_waitq_t *q) {
    KERNEL_REQUIRE(q != 0);

    if (q->bitmap == 0u) {
        return 0;
    }

    uint32_t p = highest_prio_from_bitmap(q->bitmap);

    KERNEL_REQUIRE(p < OS_MAX_PRIORITIES);

    kernel_task_t *task = task_list_pop_front(&q->prio[p]);

    KERNEL_REQUIRE(task != 0);

    if (task_list_is_empty(&q->prio[p])) {
        q->bitmap &= ~(1u << p);
    }

    return task;
}

kernel_task_t *prio_waitq_peek_highest(prio_waitq_t *q) {
    KERNEL_REQUIRE(q != 0);

    if (q->bitmap == 0u) {
        return 0;
    }

    uint32_t p = highest_prio_from_bitmap(q->bitmap);

    KERNEL_REQUIRE(p < OS_MAX_PRIORITIES);

    return task_list_peek_front(&q->prio[p]);
}

void prio_waitq_remove(prio_waitq_t *q, kernel_task_t *task) {
    KERNEL_REQUIRE(q != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->tcb.u8TaskPrio < OS_MAX_PRIORITIES);

    uint32_t p = task->tcb.u8TaskPrio;

    task_list_remove(&q->prio[p], task);

    if (task_list_is_empty(&q->prio[p])) {
        q->bitmap &= ~(1u << p);
    }
}