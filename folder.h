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

// folder.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int  folder_open(int x, int y, int w, int h);
void folder_handle_key(int wid, char c);
void folder_handle_click(int wid, int mx, int my);
int  folder_is_folder(int wid);
void folder_close(int wid);

#ifdef __cplusplus
}
#endif
