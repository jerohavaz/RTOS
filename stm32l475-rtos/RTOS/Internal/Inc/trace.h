#pragma once

#include "tcb.h"
#include "os_config.h"

#if OS_TRACE_ENABLED

void trace_init(void);
void trace_task_create(TCB_sctTCB_t *task);
void trace_task_ready(TCB_sctTCB_t *task);
void trace_task_run(TCB_sctTCB_t *task);
void trace_task_stop_run(void);
void trace_task_block(TCB_sctTCB_t *task);
void trace_idle(void);
void trace_isr_enter(void);
void trace_isr_exit(void);
void trace_isr_exit_to_scheduler(void);
void trace_log(const char *text);

#else

static inline void trace_init(void) {}
static inline void trace_task_create(TCB_sctTCB_t *task) {}
static inline void trace_task_ready(TCB_sctTCB_t *task) {}
static inline void trace_task_run(TCB_sctTCB_t *task) {}
static inline void trace_task_stop_run(void) {}
static inline void trace_task_block(TCB_sctTCB_t *task) {}
static inline void trace_idle(void) {}
static inline void trace_isr_enter(void) {}
static inline void trace_isr_exit(void) {}
static inline void trace_isr_exit_to_scheduler(void) {}
static inline void trace_log(const char *text) {}

#endif