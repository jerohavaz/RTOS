#ifndef OS_ISR_H_
#define OS_ISR_H_

#include <stdint.h>

void os_isr_enter(void);
void os_systick_tick(void);
void os_isr_exit(void);

#endif