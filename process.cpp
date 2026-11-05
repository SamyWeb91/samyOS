// process.cpp
#include "process.h"
#include "screen.h"

static Process process_table[MAX_PROCESSES];

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].active = 0;
        process_table[i].pid = 0;
        process_table[i].name[0] = 0;
    }
    kprintf("Process: Tabla inicializada (max %d)\n", MAX_PROCESSES);
}

int process_alloc(const char *name, uint32_t *out_base, uint32_t *out_stack) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_table[i].active) {
            process_table[i].active = 1;
            process_table[i].pid = i + 1; // PIDs empiezan en 1
            
            // Asignar dirección base
            uint32_t base = USER_MEM_START + (i * USER_MEM_BLOCK);
            process_table[i].base_addr = base;
            process_table[i].user_stack = base + 0x10000; // Pila a 64KB del inicio del bloque
            
            // Copiar nombre
            int k = 0;
            while (name[k] && k < PROC_NAME_MAX - 1) {
                process_table[i].name[k] = name[k];
                k++;
            }
            process_table[i].name[k] = 0;

            *out_base = process_table[i].base_addr;
            *out_stack = process_table[i].user_stack;
            
            kprintf("Process: Asignado PID %d en base 0x%x (stack 0x%x)\n", 
                    process_table[i].pid, base, process_table[i].user_stack);
            return i;
        }
    }
    kprintf("Process: Error, tabla de procesos llena.\n");
    return -1;
}

void process_free(int pid) {
    if (pid < 0 || pid >= MAX_PROCESSES) return;
    if (!process_table[pid].active) return;
    
    kprintf("Process: Liberando PID %d (%s)\n", process_table[pid].pid, process_table[pid].name);
    process_table[pid].active = 0;
}
