#pragma once

#include "os_config.h"
#include "tcb.h"

#include <stdint.h>

#if OS_TRACE_ENABLED

void trace_init(void);

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

void trace_task_create(TCB_sctTCB_t *task);
void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state);

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

void trace_task_ready(TCB_sctTCB_t *task);
void trace_task_run(TCB_sctTCB_t *task);
void trace_task_stop_run(void);
void trace_task_block(TCB_sctTCB_t *task);
void trace_idle(void);
void trace_tick(uint32_t dt);

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

void trace_isr_enter(void);
void trace_isr_exit(void);
void trace_isr_exit_to_scheduler(void);

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

void trace_log(const char *text);

#else

static inline void trace_init(void) {}

/* --------------------------------------------------------------------------
 * Task events
 * -------------------------------------------------------------------------- */

static inline void trace_task_create(TCB_sctTCB_t *task) {}

static inline void trace_task_state(uint8_t task_id, uint8_t old_state, uint8_t new_state) {}

/* --------------------------------------------------------------------------
 * Scheduler events
 * -------------------------------------------------------------------------- */

static inline void trace_task_ready(TCB_sctTCB_t *task) {}

static inline void trace_task_run(TCB_sctTCB_t *task) {}

static inline void trace_task_stop_run(void) {}

static inline void trace_task_block(TCB_sctTCB_t *task) {}

static inline void trace_idle(void) {}

static inline void trace_tick(uint32_t dt) {}

/* --------------------------------------------------------------------------
 * ISR events
 * -------------------------------------------------------------------------- */

static inline void trace_isr_enter(void) {}

static inline void trace_isr_exit(void) {}

static inline void trace_isr_exit_to_scheduler(void) {}

/* --------------------------------------------------------------------------
 * Message queue events
 * -------------------------------------------------------------------------- */

static inline void trace_queue_create(uint32_t queue_id, uint32_t capacity) {}

static inline void trace_queue_send_attempt(uint32_t queue_id,
                                            uint8_t task_id,
                                            uint8_t task_priority,
                                            uint32_t timeout_ticks,
                                            uint32_t message_hash) {}

static inline void trace_queue_send_success(uint32_t queue_id,
                                            uint8_t task_id,
                                            uint32_t message_hash) {}

static inline void trace_queue_send_block(uint32_t queue_id,
                                          uint8_t task_id,
                                          uint8_t task_priority) {}

static inline void trace_queue_send_timeout(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_receive_attempt(uint32_t queue_id,
                                               uint8_t task_id,
                                               uint8_t task_priority,
                                               uint32_t timeout_ticks) {}

static inline void trace_queue_receive_success(uint32_t queue_id,
                                               uint8_t task_id,
                                               uint32_t message_hash) {}

static inline void trace_queue_receive_block(uint32_t queue_id,
                                             uint8_t task_id,
                                             uint8_t task_priority) {}

static inline void trace_queue_receive_timeout(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_wake_sender(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_wake_receiver(uint32_t queue_id, uint8_t task_id) {}

static inline void trace_queue_handoff(uint32_t queue_id,
                                       uint8_t sender_id,
                                       uint8_t receiver_id,
                                       uint32_t message_hash) {}

static inline void trace_queue_fill(uint32_t queue_id, uint32_t fill) {}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */