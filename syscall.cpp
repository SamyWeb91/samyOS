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

#include "syscall.h"
#include "screen.h"
#include "idt.h"
#include "gfx.h"

extern "C" void isr_128();

extern "C" void syscall_init(void) {
    idt_set_gate(0x80, (unsigned int)isr_128, 0x08, 0xEE);
    kprintf("Syscall: int 0x80 registrado (DPL=3)\n");
}

extern "C" void syscall_handler_c(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    if (eax == 11) { // SYS_DRAW_RECT
        int x = ebx;
        int y = ecx;
        int w = edx;
        int h = esi;
        uint32_t color = edi;
        
        // Proteccion basica: ignorar si el rectangulo es gigante
        if (w > 0 && w < 2048 && h > 0 && h < 2048) {
            gfx_fill_rect(x, y, w, h, color);
        }
    } 
    else if (eax == 0) { // SYS_EXIT
        kprintf("\n[Syscall: exit]\n");
        while(1) { asm volatile("cli; hlt"); } 
    }
    else {
        kprintf("[Syscall desconocida: %u]\n", eax);
    }
}
