#include "task.h"
#include "screen.h"

static Task  tasks[TASK_MAX];
static Task *current = 0;
static int   count   = 0;

// Pilas estaticas -> siempre identity-mapped, no dependen de kmalloc
static uint8_t stacks[TASK_MAX][TASK_STACK_SIZE]
    __attribute__((aligned(16)));

extern "C" void task_init(void) {
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].pid   = 0;
        tasks[i].state = 0;
        tasks[i].esp   = 0;
        tasks[i].next  = 0;
    }

    // Idle (PID 0): solo existe para arrancar
    tasks[0].pid   = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].next  = &tasks[0];

    current = &tasks[0];
    count   = 1;
}

extern "C" Task *task_get_current(void) {
    return current;
}

extern "C" void task_set_current(Task *t) {
    current = t;
}

extern "C" void task_save_esp(uint32_t esp) {
    if (current) current->esp = esp;
}

extern "C" int task_create(void (*entry)(void)) {
    if (count >= TASK_MAX) return -1;

    int idx = count++;
    uint8_t *stack = stacks[idx];
    uint32_t *sp = (uint32_t *)(stack + TASK_STACK_SIZE);

    // Pila final esperada por isr_common:
    //   [sp+0]  = EDI
    //   [sp+4]  = ESI
    //   [sp+8]  = EBP
    //   [sp+12] = ESP (dummy)
    //   [sp+16] = EBX
    //   [sp+20] = EDX
    //   [sp+24] = ECX
    //   [sp+28] = EAX
    //   [sp+32] = int_no      (32)
    //   [sp+36] = err_code    (0)
    //   [sp+40] = EIP
    //   [sp+44] = CS          (0x08)
    //   [sp+48] = EFLAGS      (0x202)
    //
    // Empujamos en orden inverso:

    sp--; *sp = 0x202;              // EFLAGS (IF=1)
    sp--; *sp = 0x08;               // CS kernel
    sp--; *sp = (uint32_t)entry;    // EIP

    sp--; *sp = 0;                  // err_code
    sp--; *sp = 32;                 // int_no (IRQ0)

    sp--; *sp = 0;                  // EAX
    sp--; *sp = 0;                  // ECX
    sp--; *sp = 0;                  // EDX
    sp--; *sp = 0;                  // EBX
    sp--; *sp = 0;                  // ESP (dummy)
    sp--; *sp = 0;                  // EBP
    sp--; *sp = 0;                  // ESI
    sp--; *sp = 0;                  // EDI  <- sp apunta aqui

    tasks[idx].pid   = idx;
    tasks[idx].state = TASK_READY;
    tasks[idx].esp   = (uint32_t)sp;

    // Insertar en lista circular despues del idle
    tasks[idx].next = tasks[0].next;
    tasks[0].next   = &tasks[idx];

    kprintf("Tarea %d lista (entry=%x)\n", idx, (uint32_t)entry);
    return idx;
}

// Round-robin simple: salta el idle si hay tareas de usuario
extern "C" Task *schedule_next(void) {
    Task *c = current;
    Task *n = c->next;

    // Buscar la siguiente READY con pid != 0
    for (int i = 0; i < TASK_MAX; i++) {
        if (n->state == TASK_READY && n->pid != 0)
            break;
        n = n->next;
        if (n == c) break;
    }

    // Si no hay nadie mas, quedarse
    if (n == c || (n->state != TASK_READY && n->pid == 0))
        return c;

    if (c->state == TASK_RUNNING)
        c->state = TASK_READY;
    n->state = TASK_RUNNING;
    return n;
}
