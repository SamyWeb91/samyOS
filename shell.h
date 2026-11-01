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

// shell.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_MAX_TERMS 4

int  shell_init(void);
int  shell_new_terminal(int x, int y, int w, int h, const char *title);
void shell_handle_key(int wid, char c);
void shell_close_terminal(int wid);
int  shell_is_terminal(int wid);

/* Helpers de impresion para otros modulos (disk.cpp, folder.cpp, etc.) */
void shell_term_print(int wid, const char *s);
void shell_term_println(int wid, const char *s);
void shell_print_uint(int wid, unsigned int v);

#ifdef __cplusplus
}
#endif
