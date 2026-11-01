// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "paging.h"
#include "screen.h"

static unsigned int page_directory[1024] __attribute__((aligned(4096)));
static unsigned int page_table_0[1024]   __attribute__((aligned(4096)));

#define PT_POOL_SIZE 64   // 32 PTs = 128 MB mapeables
static unsigned int pt_pool[PT_POOL_SIZE][1024] __attribute__((aligned(4096)));
static int pt_pool_next = 0;

extern "C" void paging_enable_asm(unsigned int pd_phys);

extern "C" void paging_init() {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
        page_table_0[i]   = 0;
    }

    // Mapear los primeros 128 MB de RAM linealmente (identidad).
    int PTs_needed = 32;   // 32 PTs * 4 MB = 128 MB
    if (PTs_needed > PT_POOL_SIZE) PTs_needed = PT_POOL_SIZE;

    for (int pt_i = 0; pt_i < PTs_needed; pt_i++) {
        unsigned int *pt;
        if (pt_i == 0) {
            pt = page_table_0;
        } else {
            pt = pt_pool[pt_i];
        }

        unsigned int base_phys = pt_i * 0x400000;

        for (int i = 0; i < 1024; i++) {
            pt[i] = (base_phys + i * 0x1000) | PTE_PRESENT | PTE_RW;
        }

        page_directory[pt_i] = ((unsigned int)pt) | PTE_PRESENT | PTE_RW;
    }

    kprintf("paging: %d MB mapeados (0x0 - 0x%x)\n",
            PTs_needed * 4, PTs_needed * 0x400000);
}

extern "C" void paging_enable_now() {
    paging_enable_asm((unsigned int)page_directory);
}

extern "C" void paging_map(unsigned int virt, unsigned int phys, unsigned int flags) {
    unsigned int pd_index = virt >> 22;
    unsigned int pt_index = (virt >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PTE_PRESENT)) {
        if (pt_pool_next >= PT_POOL_SIZE) return;

        unsigned int *new_pt = pt_pool[pt_pool_next++];
        for (int i = 0; i < 1024; i++) new_pt[i] = 0;

        unsigned int pde_flags = PTE_PRESENT | PTE_RW;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        
        page_directory[pd_index] = ((unsigned int)new_pt) | pde_flags;
    } else {
        // Si el PDE ya existía (como los primeros 128 MB), le añadimos PTE_USER
        if ((flags & PTE_USER) && !(page_directory[pd_index] & PTE_USER)) {
            page_directory[pd_index] |= PTE_USER;
        }
    }

    unsigned int *pt = (unsigned int*)(page_directory[pd_index] & 0xFFFFF000);
    
    // FIX CRÍTICO: Escribir el PTE asegurando que los flags de usuario se mantengan
    // en el nivel inferior (Page Table). Si no hacemos esto, el iret fallará.
    unsigned int new_pte = (phys & 0xFFFFF000) | PTE_PRESENT;
    if (flags & PTE_RW)   new_pte |= PTE_RW;
    if (flags & PTE_USER) new_pte |= PTE_USER;
    if (flags & PTE_PAT)  new_pte |= PTE_PAT;

    pt[pt_index] = new_pte;

    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

extern "C" void paging_map_range(unsigned int virt, unsigned int phys, unsigned int size_kb, unsigned int flags) {
    for (unsigned int i = 0; i < size_kb; i++) {
        paging_map(virt + i * 0x1000, phys + i * 0x1000, flags);
    }
}

extern "C" unsigned int paging_get_pd_addr()  { return (unsigned int)page_directory; }
extern "C" unsigned int paging_get_pt0_addr() { return (unsigned int)page_table_0; }
