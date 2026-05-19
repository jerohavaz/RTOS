#ifndef SCHEDULER_H_
#define SCHEDULER_H_

void Scheduler_Init(void);
void Scheduler_Start(void);

void Scheduler_OnFirstTaskStart(void);
void Scheduler_SwitchContext(void);
void Scheduler_RequestContextSwitch(void);

void Kernel_Panic(void);

#endif