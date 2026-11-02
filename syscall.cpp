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

// =====================================================================
// Números de syscall — DEBEN COINCIDIR con user_syscalls.h y user_prog.cpp
// =====================================================================
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_READ        2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_DRAW_RECT   11
#define SYS_CLEAR       12

extern "C" void isr_128();

extern "C" void syscall_init(void) {
    // DPL=3 (0xEE) para que int 0x80 funcione desde Ring 3.
    // 0xEE = Present(1) | DPL(3) | S=0 | Type=1110 (32-bit interrupt gate)
    idt_set_gate(0x80, (unsigned int)isr_128, 0x08, 0xEE);
    kprintf("Syscall: int 0x80 registrado (DPL=3)\n");
}

// =====================================================================
// Handler de syscalls.
//
// NOTA: la firma debe coincidir EXACTAMENTE con lo que syscall.asm
// pushea antes de llamar. Si syscall.asm pasa (eax, ebx, ecx, edx),
// entonces solo debes declarar esos 4. Si pasa más, ajústalo.
// =====================================================================
extern "C" void syscall_handler_c(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    switch (eax) {

    case SYS_EXIT: {
        kprintf("\n[Syscall: exit(code=%u)]\n", ebx);
        // Bucle seguro: deshabilitar interrupciones y detener CPU
        while (1) { asm volatile("cli; hlt"); }
        break;
    }

    case SYS_WRITE: {
        // write(fd=ebx, buf=ecx, len=edx)
        // Por ahora solo soportamos fd=1 (stdout) → kprintf
        if (ebx == 1) {
            const char *buf = (const char *)ecx;
            for (uint32_t i = 0; i < edx; i++) {
                // kprintf carácter a carácter (o podrías usar un kputc)
                char s[2] = { buf[i], 0 };
                kprintf("%s", s);
            }
        }
        break;
    }

    case SYS_DRAW_RECT: {
        // draw_rect(x=ebx, y=ecx, w=edx, h=?...) — necesitas más args
        // Si tu stub solo pasa 4 args, no puedes pasar color. Ajusta el stub.
        int x = (int)ebx;
        int y = (int)ecx;
        int w = (int)edx;
        if (w > 0 && w < 2048) {
            // Placeholder: si tienes altura/color, pásalos por pila o args extra
            gfx_fill_rect(x, y, w, 1, 0x00FF0000); // 1px alto, rojo
        }
        break;
    }

    case SYS_CLEAR: {
        // clear(color=ebx)
        // gfx_clear(ebx);
        break;
    }

    default: {
        kprintf("[Syscall desconocida: %u]\n", eax);
        break;
    }
    }
}
