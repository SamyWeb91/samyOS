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

// Inicializa el ratón PS/2.
// Retorna 1 si OK, 0 si falla.
int mouse_init(void);

// Retorna 1 si el ratón está listo.
int mouse_is_ready(void);

// Posición actual del cursor.
int mouse_get_x(void);
int mouse_get_y(void);

// Estado de los botones.
int mouse_left_pressed(void);
int mouse_right_pressed(void);

// Handler de IRQ12 (llamado desde isr_handler).
void mouse_handler(void);

#ifdef __cplusplus
}
#endif
