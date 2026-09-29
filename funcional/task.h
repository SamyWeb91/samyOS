#pragma once

#include <stdint.h>

#define TASK_MAX       16
#define TASK_STACK_SIZE 8192    // 8 KB por tarea

// Estados de tarea
#define TASK_FREE      0
#define TASK_READY     1
#define TASK_RUNNING   2
#define TASK_BLOCKED   3
#define TASK_ZOMBIE    4

struct Task {
    uint32_t pid;
    uint32_t state;
    uint32_t esp;               // Pila actual
    uint32_t stack_base;        // Base de la pila (para liberar)
    uint32_t eip;               // Punto de entrada
    Task    *next;              // Siguiente tarea (lista enlazada)
};

void task_init();
int  task_create(void (*entry)());
Task* task_get_current();
Task* task_get_list();

#ifdef __cplusplus
extern "C" {
#endif

// Funcion en ensamblador para el context switch
void task_switch(Task *old_task, Task *new_task);

#ifdef __cplusplus
}
#endif
