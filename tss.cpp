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

#include "tss.h"
#include "gdt.h"
#include "screen.h"

// Alineada a 4 bytes (importante en hardware real)
static TSS tss __attribute__((aligned(4)));

extern "C" void tss_flush(void);

extern "C" void tss_init(void) {
    // Limpiar toda la estructura
    uint8_t *p = (uint8_t*)&tss;
    for (unsigned int i = 0; i < sizeof(TSS); i++) p[i] = 0;

    tss.ss0 = 0x10;                 // Data segment del kernel
    tss.esp0 = 0;                   // Se rellena justo antes de saltar a Ring 3
    tss.iomap_base = sizeof(TSS);   // No hay I/O bitmap

    // Registrar en la GDT (índice 5 → selector 0x28)
    gdt_set_tss(5, (uint32_t)&tss, sizeof(TSS) - 1);

    tss_flush();

    kprintf("TSS: cargado en 0x28 (base=0x%x, limit=%u, ss0=0x%x)\n",
            (uint32_t)&tss, sizeof(TSS) - 1, tss.ss0);
}

extern "C" void tss_set_kernel_stack(uint32_t esp0) {
    // Alinear a 16 bytes por si acaso
    tss.esp0 = esp0 & ~0xFu;
}
