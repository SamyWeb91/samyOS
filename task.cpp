#include "task.h"
#include "heap.h"
#include "screen.h"

static Task tasks[TASK_MAX];
static Task *current_task = 0;
static int task_count = 1;   // tarea 0 = kernel

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
    task_count = 1;
}

extern "C" Task* task_get_current() {
    return current_task;
}

extern "C" Task* task_get_list() {
    return tasks;
}

extern "C" int task_create(void (*entry)()) {
    if (task_count >= TASK_MAX) return -1;

    int idx = task_count++;

    // Alojar pila
    unsigned char *stack = (unsigned char*)kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        task_count--;
        return -1;
    }

    // La pila crece hacia abajo, asi que el ESP inicial apunta al final
    uint32_t *sp = (uint32_t*)(stack + TASK_STACK_SIZE);

    // Preparar la pila como si la tarea hubiera sido interrumpida
    // Orden de push (de arriba hacia abajo):
    //   [EFLAGS]
    //   [CS]
    //   [EIP] = entry
    //   [EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI]  (pusha)
    //   [EFLAGS]  (pushf)

    // pushf
    sp--; *sp = 0x202;                 // EFLAGS con IF=1

    // pusha (en orden inverso al popa)
    sp--; *sp = 0;   // EDI
    sp--; *sp = 0;   // ESI
    sp--; *sp = 0;   // EBP
    sp--; *sp = 0;   // ESP (ignorado por popa)
    sp--; *sp = 0;   // EBX
    sp--; *sp = 0;   // EDX
    sp--; *sp = 0;   // ECX
    sp--; *sp = 0;   // EAX

    // La direccion de retorno del task_switch
    // task_switch hace 'ret' al final, asi que ponemos el EIP de la tarea
    sp--; *sp = (uint32_t)entry;

    // Guardar el ESP
    tasks[idx].pid = idx;
    tasks[idx].state = TASK_READY;
    tasks[idx].esp = (uint32_t)sp;
    tasks[idx].stack_base = (uint32_t)stack;
    tasks[idx].eip = (uint32_t)entry;

    // Añadir a la lista circular
    tasks[idx].next = tasks[0].next;
    tasks[0].next = &tasks[idx];

    kprintf("Tarea creada: PID %d en %x (stack=%x)\n",
            idx, (uint32_t)entry, (uint32_t)stack);

    return idx;
}
