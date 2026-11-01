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

// Arranca el escritorio. Bloquea (bucle principal).
void desktop_run(void);

// Redirección del terminal gráfico (usadas por screen.cpp)
void desktop_terminal_clear(void);
void desktop_terminal_putchar(char c);

#ifdef __cplusplus
}
#endif
