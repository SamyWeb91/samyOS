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

#define WIN32_MAX        8
#define WIN32_TITLE_MAX  32
#define WIN32_COLS       48
#define WIN32_ROWS       18

#define SYS_WIN_CREATE   30
#define SYS_WIN_DESTROY  31
#define SYS_WIN_CLEAR    32
#define SYS_WIN_WRITE    33
#define SYS_WIN_RECT     34
#define SYS_WIN_TITLE    35

void win_init(void);
void win_redraw_all(void);

/* Syscalls */
int  win_sys_create(int x, int y, int w, int h, const char *title);
int  win_sys_destroy(int id);
int  win_sys_clear(int id);
int  win_sys_write(int id, const char *str, int len);
int  win_sys_rect(int id, int x, int y, int w, int h, uint32_t color);
int  win_sys_title(int id, const char *title);
/* --- NUEVO: Scroll --- */
#define WIN32_SCROLL_LINES  64   /* Historial total de líneas */
#define WIN32_VISIBLE_ROWS  WIN32_ROWS

/* Funciones de scroll */
int  win_sys_scroll_up(int id);
int  win_sys_scroll_down(int id);
int  win_sys_scroll_reset(int id);
int  win_sys_get_scroll(int id);
int  win_sys_set_scroll(int id, int offset);

/* Desktop helpers */
void win_handle_click(int mx, int my, int button);
int  win_get_focused(void);
void win_set_focused(int id);
int  win_get_rect(int id, int *x, int *y, int *w, int *h);  /* 0 = ok */
/* --- NUEVAS (para el compositor) --- */
int win_focus(int id);                 /* trae al frente + foco */
int win_move(int id, int x, int y);    /* mueve a (x,y) */
#ifdef __cplusplus
}
#endif
