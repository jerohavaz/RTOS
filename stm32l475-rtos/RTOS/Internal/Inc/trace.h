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
 * Delay events
 * -------------------------------------------------------------------------- */
void trace_task_delay_busy_start(TCB_sctTCB_t *task, uint32_t delay_ticks);
void trace_task_delay_busy_end(TCB_sctTCB_t *task);

/* --------------------------------------------------------------------------
 * Project: 3D-Gyro-Accelerometer events 
 * -------------------------------------------------------------------------- */
 void trace_sensor_read(void);
 void trace_transmission_complete(void);
 
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
 * Delay events
 * -------------------------------------------------------------------------- */
static inline void trace_task_delay_busy_start(TCB_sctTCB_t *task) {}
static inline void trace_task_delay_busy_end(TCB_sctTCB_t *task) {}

/* --------------------------------------------------------------------------
 * Generic log event
 * -------------------------------------------------------------------------- */

static inline void trace_log(const char *text) {}

#endif /* OS_TRACE_ENABLED */