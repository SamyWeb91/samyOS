#pragma once

// =====================================================================
// struct Regs — DEBE coincidir EXACTAMENTE con el layout de la pila
// que construye isr_common en isr.asm.
//
// Orden en la pila (de menor a mayor dirección), con la Opción B:
//
//   [gs]  [fs]  [es]  [ds]                 <- push ds/es/fs/gs (8 bytes)
//   [edi] [esi] [ebp] [esp_dummy]
//   [ebx] [edx] [ecx] [eax]                <- pusha (32 bytes)
//   [int_no] [err_code]                    <- stub (8 bytes)
//   [eip] [cs] [eflags] [useresp] [ss]     <- CPU (20 bytes)
//
// NOTA: los segmentos se empujan como dwords (4 bytes cada uno) aunque
// solo los 16 bits bajos son significativos. Los 16 bits altos son
// basura; usa (r->ds & 0xFFFF) para leer el selector real.
// =====================================================================
struct Regs {
    // Segmentos (empujados por push ds/es/fs/gs en isr.asm)
    unsigned int gs, fs, es, ds;

    // Registros de propósito general (pusha, en orden inverso)
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;

    // Empujados por el stub de isr.asm
    unsigned int int_no, err_code;

    // Empujados por la CPU al entrar a la excepción
    unsigned int eip, cs, eflags, useresp, ss;
};

extern "C" void isr_handler(Regs *r);
