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

// composer.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa el compositor. Llamar despues de gfx_backbuffer_init()
   y mouse_init(), antes de crear la terminal. */
void composer_init(void);

/* Registra la ventana de la terminal (unica ventana conocida por
   el compositor de antemano). */
void composer_set_terminal(int wid);

/* Procesa el raton: posicion, botones, drag de ventanas.
   Devuelve 1 si hubo cambios visuales. */
int composer_handle_mouse(void);

/* Procesa un caracter de teclado. */
void composer_handle_key(char c);

/* Refresco periodico (reloj, CPU). Llamar cada iteracion del bucle.
   Devuelve 1 si marco algo sucio. */
int composer_periodic(unsigned int now_ticks);

/* ¿Hay algo sucio pendiente de redibujar? */
int composer_has_dirty(void);

/* Aplica el dirty: redibuja y flipea. */
void composer_apply(void);

/* Marca una ventana entera como sucia (para redibujarla). */
void composer_dirty_window(int wid);

/* Fuerza un redibujado completo. */
void composer_force_redraw(void);

#ifdef __cplusplus
}
#endif
