// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "thread.h"
#include "screen.h"

extern "C" void context_switch(uint32_t *old_esp, uint32_t new_esp);
extern "C" void thread_entry_trampoline(void);
extern "C" void user_mode_entry(uint32_t eip, uint32_t esp, uint32_t eflags, uint32_t cs, uint32_t ss);

static Thread threads[THREAD_MAX];
static int    thread_count = 0;
static int    current = 0;
static volatile int in_schedule = 0;

static uint8_t thread_stacks[THREAD_MAX][THREAD_STACK] __attribute__((aligned(16)));
static void (*thread_entries[THREAD_MAX])(void);
static unsigned int thread_ticks[THREAD_MAX];

extern "C" void thread_trampoline_c(void) {
    int id = current;
    void (*fn)(void) = thread_entries[id];
    thread_entries[id] = 0;
    if (fn) fn();
    thread_exit();
    while (1) {}
}

extern "C" void thread_init(void) {
    for (int i = 0; i < THREAD_MAX; i++) {
        threads[i].esp = 0;
        threads[i].state = TH_ZOMBIE;
        threads[i].name[0] = 0;
        thread_entries[i] = 0;
        thread_ticks[i] = 0;
        threads[i].user_eip = 0;
    }

    current = 0;
    threads[0].state = TH_RUNNING;
    threads[0].id = 0;
    threads[0].priority = 1;
    
    uint32_t kmain_esp;
    asm volatile("mov %%esp, %0" : "=r"(kmain_esp));
    threads[0].esp = kmain_esp;
    
    const char *n = "kmain";
    int k = 0; while (n[k] && k < THREAD_NAME_MAX - 1) { threads[0].name[k] = n[k]; k++; }
    threads[0].name[k] = 0;
    thread_count = 1;

    kprintf("Threads: init (%d max) kmain esp=%x\n", THREAD_MAX, kmain_esp);
}

extern "C" int thread_create(const char *name, void (*entry)(void)) {
    if (thread_count >= THREAD_MAX || !entry) return -1;
    int idx = -1;
    for (int i = 1; i < THREAD_MAX; i++) {
        if (threads[i].state == TH_ZOMBIE && threads[i].name[0] == 0) { idx = i; break; }
    }
    if (idx < 0) return -1;

    uint8_t *stack = thread_stacks[idx];
    int k = 0; while (name[k] && k < THREAD_NAME_MAX - 1) { threads[idx].name[k] = name[k]; k++; }
    threads[idx].name[k] = 0;
    
    threads[idx].id = idx;
    threads[idx].stack_base = (uint32_t)stack;
    threads[idx].stack_size = THREAD_STACK;
    threads[idx].state = TH_READY;
    threads[idx].priority = 1;
    thread_entries[idx] = entry;
    thread_ticks[idx] = 0;
    threads[idx].user_eip = 0; // No es de usuario

    uint32_t *sp = (uint32_t *)(stack + THREAD_STACK);
    *--sp = (uint32_t)thread_entry_trampoline;
    *--sp = 0x202;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;

    threads[idx].esp = (uint32_t)sp;
    thread_count++;
    return idx;
}

// =====================================================================
// Crear hilo de Usuario (Ring 3)
// =====================================================================
extern "C" int thread_create_user(const char *name, uint32_t eip, uint32_t esp) {
    if (thread_count >= THREAD_MAX) return -1;
    int idx = -1;
    for (int i = 1; i < THREAD_MAX; i++) {
        if (threads[i].state == TH_ZOMBIE && threads[i].name[0] == 0) { idx = i; break; }
    }
    if (idx < 0) return -1;

    int k = 0; while (name[k] && k < THREAD_NAME_MAX - 1) { threads[idx].name[k] = name[k]; k++; }
    threads[idx].name[k] = 0;
    
    threads[idx].id = idx;
    threads[idx].state = TH_USER;
    threads[idx].priority = 1;
    thread_ticks[idx] = 0;
    
    // Contexto de usuario
    threads[idx].user_eip = eip;
    threads[idx].user_esp = esp;
    threads[idx].user_eflags = 0x202;
    threads[idx].user_cs = 0x1B;
    threads[idx].user_ss = 0x23;

    // Pila del kernel (para cuando entre por interrupciones/syscalls)
    uint8_t *stack = thread_stacks[idx];
    uint32_t *sp = (uint32_t *)(stack + THREAD_STACK);
    *--sp = 0; // ret dummy (no se usa en user_mode_entry)
    *--sp = 0x202; 
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    threads[idx].esp = (uint32_t)sp;
    
    thread_count++;
    kprintf("Threads: USER '%s' (id=%d) eip=%x esp=%x\n", name, idx, eip, esp);
    return idx;
}

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

    // Buscar el siguiente hilo (READY, RUNNING o USER)
    for (int i = 1; i <= THREAD_MAX; i++) {
        int c = (prev + i) % THREAD_MAX;
        if ((threads[c].state == TH_READY || threads[c].state == TH_USER) && threads[c].esp != 0) {
            next = c;
            break;
        }
    }

    if (next < 0 || next == prev) {
        in_schedule = 0;
        if (flags & 0x200) asm volatile("sti");
        return;
    }

    if (threads[prev].state == TH_RUNNING) threads[prev].state = TH_READY;
    if (threads[next].state == TH_READY || threads[next].state == TH_USER) threads[next].state = TH_RUNNING;
    
    int was_user = threads[next].user_eip != 0;

    current = next;
    in_schedule = 0;

    if (was_user) {
        // SALTO A RING 3 (iretd)
        user_mode_entry(threads[next].user_eip, threads[next].user_esp, 
                        threads[next].user_eflags, threads[next].user_cs, threads[next].user_ss);
    } else {
        // CAMBIO DE CONTEXTO EN RING 0 (ret)
        context_switch(&threads[prev].esp, threads[next].esp);
    }
    
    if (flags & 0x200) asm volatile("sti");
}

extern "C" void thread_yield(void) {
    schedule();
}

extern "C" void thread_exit(void) {
    if (current <= 0) return;

    asm volatile("cli");
    threads[current].state      = TH_ZOMBIE;
    threads[current].stack_base = 0;
    threads[current].esp        = 0;
    threads[current].stack_size = 0;
    threads[current].priority   = 0;
    threads[current].name[0]    = 0;
    threads[current].user_eip   = 0;
    thread_entries[current]     = 0;
    thread_count--;
    asm volatile("sti");

    schedule();
    while (1) {}
}

extern "C" int thread_kill(int id) {
    if (id <= 0 || id >= THREAD_MAX) return -1;
    if (threads[id].state == TH_ZOMBIE) return -1;

    asm volatile("cli");
    threads[id].state      = TH_ZOMBIE;
    threads[id].stack_base = 0;
    threads[id].esp        = 0;
    threads[id].stack_size = 0;
    threads[id].priority   = 0;
    threads[id].name[0]    = 0;
    threads[id].user_eip   = 0;
    thread_entries[id]     = 0;
    thread_count--;
    asm volatile("sti");
    return 0;
}

extern "C" int thread_current_id(void) { return current; }

extern "C" void thread_set_priority(int id, int prio) {
    if (id < 0 || id >= THREAD_MAX) return;
    if (prio < 0) prio = 0;
    if (prio > 20) prio = 20;
    threads[id].priority = prio;
}

extern "C" void thread_list(void) {
    const char *state_names[] = { "READY", "RUNNING", "BLOCKED", "ZOMBIE", "USER" };
    kprintf("PID  NOMBRE      ESTADO    PRIO  TICKS\n");
    for (int i = 0; i < THREAD_MAX; i++) {
        if (threads[i].name[0] == 0) continue;
        if (threads[i].state == TH_ZOMBIE && threads[i].esp == 0) continue;

        kprintf("  %d  %s", i, threads[i].name);
        int len = 0; while (threads[i].name[len]) len++;
        for (int j = len; j < 10; j++) screen_putchar(' ');

        kprintf("%s", state_names[threads[i].state]);
        int sl = 0; while (state_names[threads[i].state][sl]) sl++;
        for (int j = sl; j < 10; j++) screen_putchar(' ');

        kprintf("%d    %u\n", threads[i].priority, thread_ticks[i]);
    }
}
