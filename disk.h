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

// disk.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Comandos. Devuelven 1 si consumieron el comando, 0 si no. */
int  disk_cmd_hex(int wid, const char *arg);
int  disk_cmd_partitions(int wid);
int  disk_cmd_disk(int wid);

/* Registra los comandos en el shell. */
void disk_init(void);

#ifdef __cplusplus
}
#endif
