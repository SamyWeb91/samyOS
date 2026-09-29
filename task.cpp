#include "task.h"
#include "heap.h"
#include "screen.h"

static Task tasks[TASK_MAX];
static Task *current_task = 0;
static int task_count = 1;

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
    tasks[0].next = &tasks[0];
    current_task = &tasks[0];
    task_count = 1;
}

extern "C" Task* task_get_current() { return current_task; }
extern "C" Task* task_get_list() { return tasks; }

extern "C" void task_set_current(Task *t) {
    current_task = t;
}

extern "C" void task_save_esp(uint32_t esp) {
    if (current_task)
        current_task->esp = esp;
}

extern "C" int task_create(void (*entry)()) {
    if (task_count >= TASK_MAX) return -1;

    int idx = task_count++;

    unsigned char *stack = (unsigned char*)kmalloc(TASK_STACK_SIZE);
    if (!stack) { task_count--; return -1; }

    uint32_t *sp = (uint32_t*)(stack + TASK_STACK_SIZE);

    // Orden inverso al popa + iretd del isr_common
    // Pila final (de [sp+0] a [sp+48]):
    //   [sp+0]  = EDI      <- popa empieza aqui
    //   [sp+4]  = ESI
    //   [sp+8]  = EBP
    //   [sp+12] = ESP
    //   [sp+16] = EBX
    //   [sp+20] = EDX
    //   [sp+24] = ECX
    //   [sp+28] = EAX
    //   [sp+32] = int_no   (32)
    //   [sp+36] = err_code (0)
    //   [sp+40] = EIP
    //   [sp+44] = CS
    //   [sp+48] = EFLAGS

    sp--; *sp = 0x202;              // EFLAGS
    sp--; *sp = 0x08;               // CS
    sp--; *sp = (uint32_t)entry;    // EIP
    sp--; *sp = 0;                  // err_code
    sp--; *sp = 32;                 // int_no
    sp--; *sp = 0;                  // EAX
    sp--; *sp = 0;                  // ECX
    sp--; *sp = 0;                  // EDX
    sp--; *sp = 0;                  // EBX
    sp--; *sp = 0;                  // ESP
    sp--; *sp = 0;                  // EBP
    sp--; *sp = 0;                  // ESI
    sp--; *sp = 0;                  // EDI

    tasks[idx].pid = idx;
    tasks[idx].state = TASK_READY;
    tasks[idx].esp = (uint32_t)sp;
    tasks[idx].stack_base = (uint32_t)stack;
    tasks[idx].eip = (uint32_t)entry;

    tasks[idx].next = tasks[0].next;
    tasks[0].next = &tasks[idx];

    kprintf("Tarea creada: PID %d en %x (stack=%x)\n",
            idx, (uint32_t)entry, (uint32_t)stack);

    return idx;
}

extern "C" Task* schedule_next() {
    Task *current = current_task;
    Task *next = current->next;

    int guard = 0;
    while (next->state != TASK_READY && next != current && guard < TASK_MAX) {
        next = next->next;
        guard++;
    }

    if (next == current)
        return current;

    if (current->state == TASK_RUNNING)
        current->state = TASK_READY;

    next->state = TASK_RUNNING;
    return next;
}
