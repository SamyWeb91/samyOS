#include "task.h"
#include "heap.h"
#include "screen.h"

static Task tasks[TASK_MAX];
static Task *current_task = 0;
static uint32_t next_pid = 1;

extern "C" void task_init() {
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].pid = 0;
        tasks[i].state = TASK_FREE;
        tasks[i].esp = 0;
        tasks[i].stack_base = 0;
        tasks[i].eip = 0;
        tasks[i].next = 0;
    }

    tasks[0].pid = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].esp = 0;
    tasks[0].stack_base = 0;
    tasks[0].eip = 0;
    tasks[0].next = &tasks[0];

    current_task = &tasks[0];

    kprintf("Task init: tarea kernel (PID 0) creada\n");
}

extern "C" Task* task_get_current() {
    return current_task;
}

extern "C" Task* task_get_list() {
    return tasks;
}

// Stub temporal: no hace nada todavia
extern "C" void task_switch(Task *old_task, Task *new_task) {
    (void)old_task;
    (void)new_task;
}
