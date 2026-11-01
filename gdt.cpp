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

#include "gdt.h"
#include "screen.h"

// 6 entradas de 8 bytes = 48 bytes
// El descriptor TSS ocupa 2 entradas (16 bytes), así que
// necesitamos 5 entradas "normales" + 1 TSS = 6 entradas.
// Pero el TSS ocupa 2 → reservamos 7 para estar seguros.
#define GDT_ENTRIES  7

static uint64_t gdt[GDT_ENTRIES];

struct GDTDescriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static GDTDescriptor gdtr;

extern "C" void gdt_flush(uint32_t gdtr_addr);

// =====================================================================
// Helper: construir un descriptor de 8 bytes
// =====================================================================
static void gdt_set_entry(int i,
                          uint32_t base,
                          uint32_t limit,
                          uint8_t  access,
                          uint8_t  flags)
{
    gdt[i] =
        ((uint64_t)(limit & 0xFFFF)) |
        ((uint64_t)(base & 0xFFFFFF) << 16) |
        ((uint64_t)access << 40) |
        ((uint64_t)((limit >> 16) & 0x0F) << 48) |
        ((uint64_t)(flags & 0xF0) << 48) |
        ((uint64_t)((base >> 24) & 0xFF) << 56);
}

// =====================================================================
// Init
// =====================================================================
extern "C" void gdt_init(void) {
    // 0: null
    gdt[0] = 0;

    // 1: kernel code (ring 0)
    //    access = 0x9A = present, DPL=0, code, exec/read
    //    flags  = 0xCF = gran=4K, 32-bit, limit[19:16]=0xF
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // 2: kernel data (ring 0)
    //    access = 0x92 = present, DPL=0, data, read/write
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

    // 3: user code (ring 3)
    //    access = 0xFA = present, DPL=3, code, exec/read
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);

    // 4: user data (ring 3)
    //    access = 0xF2 = present, DPL=3, data, read/write
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);

    // 5-6: reservados para el TSS (se llenan en tss_init)
    gdt[5] = 0;
    gdt[6] = 0;

    // Cargar la GDT
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint32_t)&gdt[0];

    gdt_flush((uint32_t)&gdtr);

    kprintf("GDT: cargada (%d entradas)\n", GDT_ENTRIES);
}

// =====================================================================
// Descriptor TSS (2 entradas, 16 bytes)
// =====================================================================
extern "C" void gdt_set_tss(int index, uint32_t base, uint32_t limit) {
    // OJO: index es el índice del descriptor TSS.
    // El TSS ocupa 2 entradas de 8 bytes consecutivas.
    uint8_t *desc = (uint8_t*)&gdt[index];

    // Bytes 0-1: límite[15:0]
    desc[0] = limit & 0xFF;
    desc[1] = (limit >> 8) & 0xFF;

    // Bytes 2-4: base[23:0]
    desc[2] = base & 0xFF;
    desc[3] = (base >> 8) & 0xFF;
    desc[4] = (base >> 16) & 0xFF;

    // Byte 5: acceso = 0x89 (present, DPL=0, tipo=1001 TSS 32-bit disponible)
    desc[5] = 0x89;

    // Byte 6: límite[19:16] (bits 0-3) + flags (bits 4-7)
    //         flags = 0x00 (TSS no usa granularidad de página)
    desc[6] = (limit >> 16) & 0x0F;

    // Byte 7: base[31:24]
    desc[7] = (base >> 24) & 0xFF;

    // Bytes 8-15: base[32:63] = 0 (no usado en 32-bit)
    for (int i = 8; i < 16; i++) desc[i] = 0;
}
