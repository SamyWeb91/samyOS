// process.h
#pragma once
#include <stdint.h>

#define MAX_PROCESSES 16
#define PROC_NAME_MAX 32

#define USER_MEM_START 0x08000000
#define USER_MEM_BLOCK 0x01000000  // 16 MB por proceso

typedef struct {
    uint32_t pid;
    uint32_t base_addr;      // Dirección base virtual de este proceso
    uint32_t entry_point;
    uint32_t user_stack;
    int      active;         // 1 si está corriendo, 0 si está libre
    char     name[PROC_NAME_MAX];
} Process;

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa la tabla de procesos
void process_init(void);

// Busca un hueco y asigna un bloque de memoria. Devuelve el índice o -1.
int process_alloc(const char *name, uint32_t *out_base, uint32_t *out_stack);

// Libera un proceso cuando termina
void process_free(int pid);

#ifdef __cplusplus
}
#endif
