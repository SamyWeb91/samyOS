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

/*
 * Layout EXACTO de la pila cuando isr_common llama a isr_handler.
 *
 * Orden de push en isr_common (de arriba a abajo en la pila):
 *
 *   CPU pushea (si cambia de ring):  SS, ESP, EFLAGS, CS, EIP
 *   CPU pushea (si la excepción tiene error code): err_code
 *   Stub pushea:  int_no   (push num)
 *   Stub pushea:  err_code dummy (push 0)  [solo si NO lo puso el CPU]
 *   isr_common:   pusha -> EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
 *   isr_common:   push ds, es, fs, gs
 *
 * En memoria (direcciones crecientes), el frame queda así:
 *
 *   [ESP+0]  = GS
 *   [ESP+4]  = FS
 *   [ESP+8]  = ES
 *   [ESP+12] = DS
 *   [ESP+16] = EDI
 *   [ESP+20] = ESI
 *   [ESP+24] = EBP
 *   [ESP+28] = ESP (valor antes de pusha)
 *   [ESP+32] = EBX
 *   [ESP+36] = EDX
 *   [ESP+40] = ECX
 *   [ESP+44] = EAX
 *   [ESP+48] = int_no
 *   [ESP+52] = err_code
 *   [ESP+56] = EIP
 *   [ESP+60] = CS
 *   [ESP+64] = EFLAGS
 *   [ESP+68] = useresp   (solo si cambió de ring)
 *   [ESP+72] = ss        (solo si cambió de ring)
 *
 * El struct Regs debe reflejar EXACTAMENTE ese layout.
 */
struct Regs {
    // Guardados por isr_common (en este orden en memoria)
    unsigned int gs, fs, es, ds;
    // Guardados por pusha (en este orden en memoria)
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    // Pusheados por el stub
    unsigned int int_no, err_code;
    // Pusheados por la CPU
    unsigned int eip, cs, eflags, useresp, ss;
};

extern "C" void isr_handler(Regs *r);
