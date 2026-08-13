/**
 * @file timeout_list.c
 * @brief Ordered kernel timeout-list implementation.
 * @author Jerome
 */

#include "timeout_list.h"
#include "kernel_panic.h"
#include "kernel_task.h"
#include "task_list.h"
#include <stdbool.h>

/**
 * @brief Select a task's dedicated timeout-list node.
 *
 * @param task Task whose timeout node is required.
 *
 * @return Pointer to @c task->timeout_node.
 *
 * @pre @p task must not be null.
 */
static kernel_task_list_node_t *timeout_node(kernel_task_t *task) {
    KERNEL_REQUIRE(task != 0);

    return &task->timeout_node;
}

void timeout_list_init(timeout_list_t *timeout_list) {
    KERNEL_REQUIRE(timeout_list != 0);

    task_list_init(&timeout_list->list, timeout_node);
}

void timeout_list_add(timeout_list_t *timeout_list, kernel_task_t *task, uint32_t wake_tick) {
    KERNEL_REQUIRE(timeout_list != 0);
    KERNEL_REQUIRE(task != 0);
    KERNEL_REQUIRE(task->wake_tick == 0u);

    task->wake_tick = wake_tick;

    if (task_list_is_empty(&timeout_list->list)) {
        task_list_push_back(&timeout_list->list, task);
        return;
    }

    KERNEL_REQUIRE(timeout_list->list.head != 0);
    KERNEL_REQUIRE(timeout_list->list.count > 0u);
    KERNEL_REQUIRE(timeout_list->list.get_node != 0);

    kernel_task_t *current = timeout_list->list.head;

    for (uint32_t i = 0u; i < timeout_list->list.count; i++) {
        KERNEL_REQUIRE(current != 0);

        if ((int32_t)(wake_tick - current->wake_tick) < 0) {
            task_list_insert_before(&timeout_list->list, current, task);
            return;
        }

        kernel_task_list_node_t *node = timeout_list->list.get_node(current);

        KERNEL_REQUIRE(node != 0);
        KERNEL_REQUIRE(node->next != 0);

        current = node->next;
    }

    task_list_push_back(&timeout_list->list, task);
}

void timeout_list_remove(timeout_list_t *timeout_list, kernel_task_t *task) {
    KERNEL_REQUIRE(timeout_list != 0);
    KERNEL_REQUIRE(task != 0);

    task_list_remove(&timeout_list->list, task);
    task->wake_tick = 0;
}

bool timeout_list_try_remove(timeout_list_t *timeout_list, kernel_task_t *task) {
    KERNEL_REQUIRE(timeout_list != 0);
    KERNEL_REQUIRE(task != 0);

    bool removed = task_list_try_remove(&timeout_list->list, task);

    if (removed) {
        task->wake_tick = 0u;
    }

    return removed;
}

kernel_task_t *timeout_list_pop_expired(timeout_list_t *timeout_list, uint32_t now) {
    KERNEL_REQUIRE(timeout_list != 0);

    if (task_list_is_empty(&timeout_list->list)) {
        return 0;
    }

    KERNEL_REQUIRE(timeout_list->list.head != 0);

    kernel_task_t *task = timeout_list->list.head;

    /*
     * Wrap-safe for delays smaller than 2^31 ticks.
     */
    if ((int32_t)(now - task->wake_tick) < 0) {
        return 0;
    }

    task = task_list_pop_front(&timeout_list->list);
    KERNEL_REQUIRE(task != 0);

    task->wake_tick = 0u;

    return task;
}