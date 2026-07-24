#ifndef OS_DELAY_H_
#define OS_DELAY_H_

#include "os_types.h"
#include <stdint.h>

os_status_t os_delay(uint32_t delay_ticks);
void os_delay_busy(uint32_t delay_ticks);

#endif