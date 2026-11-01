/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#define THREAD_MAX       8
#define THREAD_STACK     4096
#define THREAD_NAME_MAX  32

// Estados
#define TH_READY    0
#define TH_RUNNING  1
#define TH_BLOCKED  2
#define TH_ZOMBIE   3
#define TH_USER     4  // <--- NUEVO: Hilo en Ring 3

struct Thread {
    uint32_t esp;              // stack pointer guardado (kernel)
    uint32_t stack_base;       
    uint32_t stack_size;
    int      state;
    int      id;
    int      priority;
    char     name[THREAD_NAME_MAX];
    
    // NUEVO: Contexto para volver a Ring 3
    uint32_t user_eip;
    uint32_t user_esp;
    uint32_t user_eflags;
    uint32_t user_cs;
    uint32_t user_ss;
};

#ifdef __cplusplus
extern "C" {
#endif

void  thread_init(void);
int   thread_create(const char *name, void (*entry)(void));
int   thread_create_user(const char *name, uint32_t eip, uint32_t esp); // <--- NUEVO
void  thread_yield(void);
void  thread_exit(void);
int   thread_kill(int id);
int   thread_current_id(void);
void  thread_list(void);
void  schedule(void);

int   thread_get_count(void);
int   thread_get_state(int id);

#ifdef __cplusplus
}
#endif
