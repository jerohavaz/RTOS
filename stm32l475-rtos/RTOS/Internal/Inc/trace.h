#pragma once

#include "tcb.h"

#ifndef TRACE_ENABLED
#define TRACE_ENABLED 0
#endif

#if TRACE_ENABLED

void trace_init(void);
void trace_task_create(tcb_t *task);
void trace_task_ready(tcb_t *task);
void trace_task_run(tcb_t *task);
void trace_task_stop_run(void);
void trace_task_block(tcb_t *task);
void trace_idle(void); // TODO: TRACE IDLE?
void trace_isr_enter(void);
void trace_isr_exit(void);
void trace_isr_exit_to_scheduler(void);
void trace_log(const char *text);

#else

static inline void trace_init(void) {}
static inline void trace_task_create(tcb_t *task) {}
static inline void trace_task_ready(tcb_t *task) {}
static inline void trace_task_run(tcb_t *task) {}
static inline void trace_task_stop_run(void) {}
static inline void trace_task_block(tcb_t *task) {}
static inline void trace_isr_enter(void) {}
static inline void trace_isr_exit(void) {}
static inline void trace_isr_exit_to_scheduler(void) {}
static inline void trace_log(const char *text) {}

#endif