#pragma once

#include <stdint.h>

#define TASK_MAX        16
#define TASK_STACK_SIZE 8192

#define TASK_FREE      0
#define TASK_READY     1
#define TASK_RUNNING   2
#define TASK_BLOCKED   3
#define TASK_ZOMBIE    4

struct Task {
    uint32_t pid;
    uint32_t state;
    uint32_t esp;
    uint32_t stack_base;
    uint32_t eip;
    Task    *next;
};

#ifdef __cplusplus
extern "C" {
#endif

void task_init();
int  task_create(void (*entry)());
Task* task_get_current();
Task* task_get_list();
void task_switch(Task *old_task, Task *new_task);

#ifdef __cplusplus
}
#endif
