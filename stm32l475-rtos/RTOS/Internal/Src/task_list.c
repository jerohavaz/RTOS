/**
 * @file task_list.c
 * @brief Task list implementation.
 * @author Jerome
 */

#include "task_list.h"
#include "kernel_panic.h"
#include <stdbool.h>

/**
 * @brief Test whether both links of an intrusive node are populated.
 *
 * @param node Node to inspect.
 *
 * @retval true @p node is non-null and both neighbor pointers are non-null.
 * @retval false @p node is null or at least one neighbor pointer is null.
 */
static bool task_list_node_is_linked(const kernel_task_list_node_t *node) {
    return (node != 0) && (node->next != 0) && (node->prev != 0);
}

/**
 * @brief Enforce the minimum initialized-list invariants.
 *
 * @param list List to validate.
 *
 * @pre @p list must not be null.
 * @pre The list's node-selector callback must not be null.
 */
static void task_list_require_valid(const task_list_t *list) {
    KERNEL_REQUIRE(list != 0);
    KERNEL_REQUIRE(list->get_node != 0);
}

/**
 * @brief Resolve the intrusive node selected by a list for a task.
 *
 * @param list Initialized list containing the node-selector callback.
 * @param task Task whose embedded node is required.
 *
 * @return Non-null node selected from @p task.
 *
 * @pre @p list must be valid.
 * @pre @p task must not be null.
 * @post The configured selector must return a non-null node.
 */
static kernel_task_list_node_t *task_list_get_node(task_list_t *list, kernel_task_t *task) {
    task_list_require_valid(list);
    KERNEL_REQUIRE(task != 0);

    kernel_task_list_node_t *node = list->get_node(task);

    KERNEL_REQUIRE(node != 0);

    return node;
}

/**
 * @brief Unlink a task known to be present in a list.
 *
 * Repairs neighboring links, advances the head when required, decrements the
 * count, and clears both links in the removed task's selected node.
 *
 * @param list Initialized non-empty list containing @p task.
 * @param task Task to unlink.
 *
 * @pre @p task must be linked through the node selected by @p list.
 * @post The selected node in @p task is unlinked.
 * @post List circularity and head/count consistency are preserved.
 */
static void task_list_unlink_present(task_list_t *list, kernel_task_t *task) {
    task_list_require_valid(list);
    KERNEL_REQUIRE(task != 0);

    KERNEL_REQUIRE(list->head != 0);
    KERNEL_REQUIRE(list->count > 0u);

    kernel_task_list_node_t *node = task_list_get_node(list, task);

    KERNEL_REQUIRE(task_list_node_is_linked(node));

    if (list->count == 1u) {
        KERNEL_REQUIRE(list->head == task);
        KERNEL_REQUIRE(node->next == task);
        KERNEL_REQUIRE(node->prev == task);

        list->head = 0;
        list->count = 0u;
        node->next = 0;
        node->prev = 0;
        return;
    }

    kernel_task_t *next = node->next;
    kernel_task_t *prev = node->prev;

    KERNEL_REQUIRE(next != 0);
    KERNEL_REQUIRE(prev != 0);
    KERNEL_REQUIRE(next != task);
    KERNEL_REQUIRE(prev != task);

    kernel_task_list_node_t *next_node = task_list_get_node(list, next);
    kernel_task_list_node_t *prev_node = task_list_get_node(list, prev);

    KERNEL_REQUIRE(task_list_node_is_linked(next_node));
    KERNEL_REQUIRE(task_list_node_is_linked(prev_node));

    prev_node->next = next;
    next_node->prev = prev;

    if (list->head == task) {
        list->head = next;
    }

    node->next = 0;
    node->prev = 0;

    list->count--;
}

void task_list_init(task_list_t *list, task_node_fn_t get_node) {
    KERNEL_REQUIRE(list != 0);
    KERNEL_REQUIRE(get_node != 0);

    list->head = 0;
    list->count = 0u;
    list->get_node = get_node;
}

bool task_list_is_empty(const task_list_t *list) {
    KERNEL_REQUIRE(list != 0);

    if (list->head == 0) {
        KERNEL_REQUIRE(list->count == 0u);
    } else {
        KERNEL_REQUIRE(list->count > 0u);
    }

    return (list->count == 0u);
}

void task_list_push_back(task_list_t *list, kernel_task_t *task) {
    kernel_task_list_node_t *node = task_list_get_node(list, task);

    KERNEL_REQUIRE(!task_list_node_is_linked(node));

    if (list->head == 0) {
        KERNEL_REQUIRE(list->count == 0u);

        /*
         * First element in a circular list points to itself.
         */
        list->head = task;
        node->next = task;
        node->prev = task;
        list->count = 1u;
        return;
    }

    KERNEL_REQUIRE(list->count > 0u);

    kernel_task_t *head = list->head;
    kernel_task_list_node_t *head_node = task_list_get_node(list, head);

    KERNEL_REQUIRE(task_list_node_is_linked(head_node));

    kernel_task_t *tail = head_node->prev;

    KERNEL_REQUIRE(tail != 0);

    kernel_task_list_node_t *tail_node = task_list_get_node(list, tail);

    KERNEL_REQUIRE(task_list_node_is_linked(tail_node));

    /*
     * Insert before head, so the new task becomes the logical tail.
     */
    node->next = head;
    node->prev = tail;

    tail_node->next = task;
    head_node->prev = task;

    list->count++;
}

void task_list_insert_before(task_list_t *list, kernel_task_t *existing, kernel_task_t *task) {
    task_list_require_valid(list);
    KERNEL_REQUIRE(task != 0);

    if ((list->head == 0) || (existing == 0)) {
        task_list_push_back(list, task);
        return;
    }

    KERNEL_REQUIRE(list->count > 0u);

    kernel_task_list_node_t *task_node = task_list_get_node(list, task);
    kernel_task_list_node_t *existing_node = task_list_get_node(list, existing);

    KERNEL_REQUIRE(!task_list_node_is_linked(task_node));
    KERNEL_REQUIRE(task_list_node_is_linked(existing_node));

    kernel_task_t *prev = existing_node->prev;

    KERNEL_REQUIRE(prev != 0);

    kernel_task_list_node_t *prev_node = task_list_get_node(list, prev);

    KERNEL_REQUIRE(task_list_node_is_linked(prev_node));

    task_node->next = existing;
    task_node->prev = prev;

    prev_node->next = task;
    existing_node->prev = task;

    if (list->head == existing) {
        list->head = task;
    }

    list->count++;
}

kernel_task_t *task_list_pop_front(task_list_t *list) {
    task_list_require_valid(list);

    if (list->head == 0) {
        KERNEL_REQUIRE(list->count == 0u);
        return 0;
    }

    KERNEL_REQUIRE(list->count > 0u);

    kernel_task_t *task = list->head;

    task_list_unlink_present(list, task);

    return task;
}

kernel_task_t *task_list_peek_front(const task_list_t *list) {
    KERNEL_REQUIRE(list != 0);

    if (list->head == 0) {
        KERNEL_REQUIRE(list->count == 0u);
        return 0;
    }

    KERNEL_REQUIRE(list->count > 0u);

    return list->head;
}

void task_list_remove(task_list_t *list, kernel_task_t *task) {
    task_list_unlink_present(list, task);
}

bool task_list_try_remove(task_list_t *list, kernel_task_t *task) {
    task_list_require_valid(list);
    KERNEL_REQUIRE(task != 0);

    if (list->head == 0) {
        KERNEL_REQUIRE(list->count == 0u);
        return false;
    }

    KERNEL_REQUIRE(list->count > 0u);

    const kernel_task_list_node_t *node = task_list_get_node(list, task);

    if (!task_list_node_is_linked(node)) {
        return false;
    }

    task_list_unlink_present(list, task);

    return true;
}