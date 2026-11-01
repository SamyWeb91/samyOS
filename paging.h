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

// paging.h
#pragma once

/* =====================================================================
 * Flags de PTE (Page Table Entry)
 * ===================================================================== */
#define PTE_PRESENT  0x001   /* Page present */
#define PTE_RW       0x002   /* Read/Write */
#define PTE_USER     0x004   /* Accesible desde ring 3 */
#define PTE_PWT      0x008   /* Page Write-Through */
#define PTE_PCD      0x010   /* Page Cache Disable */
#define PTE_PAT      0x080   /* Page Attribute Table (bit 7 en PTE) */
#define PTE_WC       (PTE_PWT | PTE_PCD)   /* Write-Combining (sin PAT) */

#ifdef __cplusplus
extern "C" {
#endif

void paging_init();
void paging_enable_now();

void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
void paging_map_range(unsigned int virt, unsigned int phys, unsigned int size_kb, unsigned int flags);

unsigned int paging_get_pd_addr();
unsigned int paging_get_pt0_addr();

#ifdef __cplusplus
}
#endif
