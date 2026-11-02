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
#include "win.h"      // Necesario para win_sys_create
#include "composer.h" // Necesario para composer_dirty_window

// =====================================================================
// Números de syscall — DEBEN COINCIDIR con el programa de usuario
// =====================================================================
#define SYS_EXIT          0
#define SYS_WRITE         1
#define SYS_READ          2
#define SYS_OPEN          3
#define SYS_CLOSE         4
#define SYS_DRAW_RECT     11
#define SYS_CLEAR         12
#define SYS_DRAW_TEXT     13
#define SYS_CREATE_WINDOW 14

extern "C" void isr_128();

// Variable externa del cargador (ring3_test.cpp)
extern "C" void ring3_volver_kernel(void);

extern "C" void syscall_init(void) {
    // DPL=3 (0xEE) para que int 0x80 funcione desde Ring 3.
    idt_set_gate(0x80, (unsigned int)isr_128, 0x08, 0xEE);
    kprintf("Syscall: int 0x80 registrado (DPL=3)\n");
}

// =====================================================================
// Handler de syscalls.
// Firma de 6 argumentos (eax, ebx, ecx, edx, esi, edi).
// =====================================================================
extern "C" void syscall_handler_c(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    switch (eax) {

    case SYS_EXIT: {
        kprintf("\n[Syscall: exit(code=%u)]\n", ebx);
        kprintf("Volviendo a la shell (Longjmp)...\n");

        // Llamar al Longjmp del cargador. 
        // Esta función no retorna: restaura el ESP y salta de vuelta al kernel.
        ring3_volver_kernel();
        
        // Por si acaso, bucle infinito de seguridad (nunca debería llegar aquí)
        while (1) { asm volatile("cli; hlt"); }
        break;
    }

    case SYS_WRITE: {
        if (ebx == 1) { // stdout
            const char *buf = (const char *)ecx;
            for (uint32_t i = 0; i < edx; i++) {
                char s[2] = { buf[i], 0 };
                kprintf("%s", s);
            }
        }
        break;
    }

    case SYS_DRAW_RECT: {
        // draw_rect(x=ebx, y=ecx, w=edx, h=esi, color=edi)
        int x = (int)ebx;
        int y = (int)ecx;
        int w = (int)edx;
        int h = (int)esi;
        uint32_t color = edi;

        if (w > 0 && w < 2048 && h > 0 && h < 2048) {
            gfx_fill_rect(x, y, w, h, color);
        }
        break;
    }

    case SYS_DRAW_TEXT: {
        // draw_text(x=ebx, y=ecx, color=edx, str=esi)
        int x = (int)ebx;
        int y = (int)ecx;
        uint32_t color = edx;
        const char *str = (const char *)esi;

        // Verificación básica de puntero (debe estar en el rango mapeado de Ring 3)
        if (str && (uint32_t)str >= 0x08000000 && (uint32_t)str < 0x08400000) {
            // Tu gfx.h requiere (x, y, string, fg_color, bg_color)
            // Usaremos 0 como fondo transparente (o negro si tu driver lo requiere)
            gfx_draw_string(x, y, str, color, 0); 
        }
        break;
    }

    case SYS_CREATE_WINDOW: {
        // create_window(x=ebx, y=ecx, w=edx, h=esi, title=edi)
        int x = (int)ebx;
        int y = (int)ecx;
        int w = (int)edx;
        int h = (int)esi;
        const char *title = (const char *)edi;

        if (title && (uint32_t)title >= 0x08000000 && (uint32_t)title < 0x08400000) {
            if (w > 0 && w < 1024 && h > 0 && h < 768) {
                // Crea una ventana nativa en el compositor del kernel
                int wid = win_sys_create(x, y, w, h, title);
                if (wid >= 0) {
                    win_focus(wid);
                    composer_dirty_window(wid);
                }
            }
        }
        break;
    }

    case SYS_CLEAR: {
        // clear(color=ebx) — placeholder
        break;
    }

    default: {
        kprintf("[Syscall desconocida: %u]\n", eax);
        break;
    }
    }
}
