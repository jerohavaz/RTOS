#ifndef PORT_H_
#define PORT_H_

#include <stdint.h>

void port_start_first_task(void);
void port_request_context_switch(void);

uint32_t port_enter_critical(void);
void port_exit_critical(uint32_t primask);

void port_wait_for_interrupt(void);

#endif