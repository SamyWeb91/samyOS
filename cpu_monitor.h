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

// cpu_monitor.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa el monitor. Llamar una vez al arrancar el desktop. */
void cpu_monitor_init(void);

/* Marca el inicio de una iteración del bucle principal. */
void cpu_monitor_begin_iter(void);

/* Marca que la CPU va a dormir (justo antes de hlt). */
void cpu_monitor_sleep_start(void);

/* Marca que la CPU ha salido de hlt (justo después). */
void cpu_monitor_sleep_end(void);

/* Cierra la iteración y actualiza el porcentaje si toca. */
void cpu_monitor_end_iter(void);

/* Devuelve el último porcentaje calculado (0..100). */
int  cpu_monitor_usage(void);

#ifdef __cplusplus
}
#endif
