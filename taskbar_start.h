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

//code
#pragma once
#define START_ACT_NONE       0
#define START_ACT_TERMINAL   1
#define START_ACT_ABOUT      2
#define START_ACT_REBOOT     3
#define START_ACT_CLOSE_ALL  4
#ifdef __cplusplus
extern "C" {
#endif
/* Inicializa el modulo. Registra el callback para las acciones del menu. */
void taskbar_start_init(void (*action_cb)(int));

/* Devuelve 1 si el click fue consumido por el boton o el menu. */
int  taskbar_start_click(int mx, int my);

/* Devuelve 1 si el hover cambio algo visual. */
int  taskbar_start_hover(int mx, int my);

/* Dibuja el boton + menu si esta abierto. */
void taskbar_start_draw(void);

/* Devuelve 1 si el menu esta abierto. */
int  taskbar_start_is_open(void);

/* Fuerza el cierre del menu (por ejemplo, al perder el foco). */
void taskbar_start_close(void);

#ifdef __cplusplus
}
#endif
