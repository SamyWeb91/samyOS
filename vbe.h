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

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa VBE leyendo la info del bootloader (0x6000).
void vbe_init(void);

// Retorna 1 si VBE está disponible y el modo se cambió.
int vbe_available(void);

// Info del modo actual.
uint32_t vbe_fb_addr(void);
uint16_t vbe_pitch(void);
uint16_t vbe_width(void);
uint16_t vbe_height(void);
uint8_t  vbe_bpp(void);

#ifdef __cplusplus
}
#endif
