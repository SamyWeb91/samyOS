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

// Direcciones en el espacio del usuario
#define USER_CODE_VADDR   0x400000
#define USER_STACK_VADDR  0x402000
#define USER_STACK_SIZE   0x2000     // 8 KB
#define USER_MSG_VADDR    0x400800
#define USER_PROG_MAX     4096       // tamaño máximo binario

#ifdef __cplusplus
extern "C" {
#endif

void user_init(void);                 // mapea código + stack del usuario
void user_start(void);                // entra en ring 3 (blob del kernel)
uint32_t user_get_kernel_stack(void); // stack del kernel para TSS.esp0

// Ejecutar un buffer en ring 3.
// Copia `size` bytes desde `buf` a 0x400000 y hace iret a ring 3.
// Bloquea hasta que el binario hace exit().
int user_run_buffer(const void *buf, uint32_t size);

// Leer 4 KB del LBA indicado y ejecutarlos en ring 3.
int user_load_and_run(uint32_t lba);

#ifdef __cplusplus
}
#endif
