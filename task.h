#pragma once

#include <stdint.h>

#define TASK_MAX        8
#define TASK_STACK_SIZE 4096

#define TASK_READY   1
#define TASK_RUNNING 2

struct Task {
    uint32_t pid;
    uint32_t state;
    uint32_t esp;       // offset 8 -> usado en isr.asm
    Task    *next;
};

#ifdef __cplusplus
extern "C" {
#endif

void  task_init(void);
int   task_create(void (*entry)(void));
Task *task_get_current(void);
void  task_set_current(Task *t);
void  task_save_esp(uint32_t esp);
Task *schedule_next(void);

#ifdef __cplusplus
}
#endif
