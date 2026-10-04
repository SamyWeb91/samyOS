#include "thread.h"
#include "screen.h"

extern "C" void context_switch(uint32_t *old_esp, uint32_t new_esp);
extern "C" void thread_entry_trampoline(void);

static Thread threads[THREAD_MAX];
static int    thread_count = 0;
static int    current = 0;
static volatile int in_schedule = 0;

static uint8_t thread_stacks[THREAD_MAX][THREAD_STACK]
    __attribute__((aligned(16)));

static void (*thread_entries[THREAD_MAX])(void);
static unsigned int thread_ticks[THREAD_MAX];

// =====================================================================
// Trampolín C
// =====================================================================
extern "C" void thread_trampoline_c(void) {
    int id = current;
    void (*fn)(void) = thread_entries[id];
    thread_entries[id] = 0;
    if (fn) fn();
    thread_exit();
    while (1) {}
}

// =====================================================================
// Init
// =====================================================================
extern "C" void thread_init(void) {
    for (int i = 0; i < THREAD_MAX; i++) {
        threads[i].esp        = 0;
        threads[i].stack_base = 0;
        threads[i].stack_size = 0;
        threads[i].state      = TH_ZOMBIE;
        threads[i].id         = i;
        threads[i].priority   = 0;
        threads[i].name[0]    = 0;
        thread_entries[i]     = 0;
        thread_ticks[i]       = 0;
    }

    current = 0;
    threads[0].state    = TH_RUNNING;
    threads[0].id       = 0;
    threads[0].priority = 1;   // RR puro: kmain igual que todos

    uint32_t kmain_esp;
    asm volatile("mov %%esp, %0" : "=r"(kmain_esp));
    threads[0].esp = kmain_esp;

    const char *n = "kmain";
    int k = 0;
    while (n[k] && k < THREAD_NAME_MAX - 1) {
        threads[0].name[k] = n[k];
        k++;
    }
    threads[0].name[k] = 0;
    thread_count = 1;

    kprintf("Threads: init (%d max) kmain esp=%x\n", THREAD_MAX, kmain_esp);
}

// =====================================================================
// Crear hilo
// =====================================================================
extern "C" int thread_create(const char *name, void (*entry)(void)) {
    if (thread_count >= THREAD_MAX || !entry)
        return -1;

    int idx = -1;
    for (int i = 1; i < THREAD_MAX; i++) {
        if (threads[i].state == TH_ZOMBIE) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    uint8_t *stack = thread_stacks[idx];

    int k = 0;
    while (name[k] && k < THREAD_NAME_MAX - 1) {
        threads[idx].name[k] = name[k];
        k++;
    }
    threads[idx].name[k] = 0;

    threads[idx].id         = idx;
    threads[idx].stack_base = (uint32_t)stack;
    threads[idx].stack_size = THREAD_STACK;
    threads[idx].state      = TH_READY;
    threads[idx].priority   = 1;
    thread_entries[idx]     = entry;
    thread_ticks[idx]       = 0;

    // Stack para popad → popfd → ret
    //   [esp+0]  EDI
    //   [esp+4]  ESI
    //   [esp+8]  EBP
    //   [esp+12] ESP dummy
    //   [esp+16] EBX
    //   [esp+20] EDX
    //   [esp+24] ECX
    //   [esp+28] EAX
    //   [esp+32] EFLAGS
    //   [esp+36] ret
    uint32_t *sp = (uint32_t *)(stack + THREAD_STACK);

    *--sp = (uint32_t)thread_entry_trampoline;  // ret
    *--sp = 0x202;  // eflags (IF=1)
    *--sp = 0;      // eax
    *--sp = 0;      // ecx
    *--sp = 0;      // edx
    *--sp = 0;      // ebx
    *--sp = 0;      // esp dummy
    *--sp = 0;      // ebp
    *--sp = 0;      // esi
    *--sp = 0;      // edi  ← esp apunta aquí

    threads[idx].esp = (uint32_t)sp;
    thread_count++;

    kprintf("Threads: creado '%s' (id=%d) esp=%x\n",
            name, idx, threads[idx].esp);
    return idx;
}

// =====================================================================
// Schedule expulsivo RR puro
//
// Flujo:
//   1. cli, guardar IF en flags
//   2. Si ya estamos en schedule → salir
//   3. Elegir siguiente READY en RR desde prev+1
//   4. in_schedule = 0 ANTES del switch (el hilo nuevo no hereda el lock)
//   5. context_switch → popfd del hilo nuevo restaura su IF
//   6. Al volver a este hilo, restaurar IF
// =====================================================================
extern "C" void schedule(void) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags));

    if (in_schedule) {
        if (flags & 0x200) asm volatile("sti");
        return;
    }
    in_schedule = 1;

    thread_ticks[current]++;

    if (thread_count <= 1) {
        in_schedule = 0;
        if (flags & 0x200) asm volatile("sti");
        return;
    }

    int prev = current;
    int next = -1;

    // RR puro: buscar el siguiente READY desde prev+1
    for (int i = 1; i <= THREAD_MAX; i++) {
        int c = (prev + i) % THREAD_MAX;
        if (threads[c].state == TH_READY && threads[c].esp != 0) {
            next = c;
            break;
        }
    }

    if (next < 0 || next == prev) {
        in_schedule = 0;
        if (flags & 0x200) asm volatile("sti");
        return;
    }

    if (threads[prev].state == TH_RUNNING)
        threads[prev].state = TH_READY;

    threads[next].state = TH_RUNNING;
    current = next;

    // in_schedule = 0 ANTES del switch.
    // El hilo nuevo tiene su propio eflags guardado (0x202 → IF=1).
    // No hacemos sti aquí: el popfd del context_switch lo hará.
    in_schedule = 0;

    context_switch(&threads[prev].esp, threads[next].esp);

    // Al volver a este hilo, el popfd ya restauró su IF.
    // Si por alguna razón quedó en 0, lo re-habilitamos.
    if (flags & 0x200) asm volatile("sti");
}

// =====================================================================
// Yield
// =====================================================================
extern "C" void thread_yield(void) {
    schedule();
}

// =====================================================================
// Exit
// =====================================================================
extern "C" void thread_exit(void) {
    if (current <= 0)
        return;

    asm volatile("cli");
    threads[current].state      = TH_ZOMBIE;
    threads[current].stack_base = 0;
    threads[current].esp        = 0;
    thread_entries[current]     = 0;
    thread_count--;
    asm volatile("sti");

    schedule();
    while (1) {}
}

// =====================================================================
// Kill
// =====================================================================
extern "C" int thread_kill(int id) {
    if (id <= 0 || id >= THREAD_MAX)
        return -1;
    if (threads[id].state == TH_ZOMBIE)
        return -1;

    asm volatile("cli");
    threads[id].state      = TH_ZOMBIE;
    threads[id].stack_base = 0;
    threads[id].esp        = 0;
    thread_entries[id]     = 0;
    thread_count--;
    asm volatile("sti");
    return 0;
}

// =====================================================================
// Info
// =====================================================================
extern "C" int thread_current_id(void) {
    return current;
}

extern "C" void thread_set_priority(int id, int prio) {
    if (id < 0 || id >= THREAD_MAX) return;
    if (prio < 0) prio = 0;
    if (prio > 20) prio = 20;
    threads[id].priority = prio;
}

extern "C" void thread_list(void) {
    const char *state_names[] = { "READY", "RUNNING", "BLOCKED", "ZOMBIE" };
    kprintf("PID  NOMBRE      ESTADO    PRIO  TICKS\n");
    for (int i = 0; i < THREAD_MAX; i++) {
        if (threads[i].state == TH_ZOMBIE && threads[i].esp == 0)
            continue;
        kprintf("  %d  ", i);
        int len = 0;
        while (threads[i].name[len]) len++;
        kprintf("%s", threads[i].name);
        for (int j = len; j < 10; j++) screen_putchar(' ');
        kprintf("%s", state_names[threads[i].state]);
        int sl = 0;
        while (state_names[threads[i].state][sl]) sl++;
        for (int j = sl; j < 10; j++) screen_putchar(' ');
        kprintf("%d    %u\n", threads[i].priority, thread_ticks[i]);
    }
}
