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

void  heap_init();
void* kmalloc(unsigned int size);
void  kfree(void* ptr);

// Estadísticas para la shell
struct HeapStats {
    unsigned int total_size;
    unsigned int used_size;
    unsigned int free_size;
    unsigned int num_blocks;
    unsigned int num_free;
};

HeapStats heap_get_stats();
