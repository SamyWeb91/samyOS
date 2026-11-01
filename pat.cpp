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

//code
// pat.cpp
#include "pat.h"
#include "screen.h"

#define MSR_IA32_PAT 0x277

extern "C" {

static inline void cpuid(unsigned int leaf,
                         unsigned int *a, unsigned int *b,
                         unsigned int *c, unsigned int *d) {
    __asm__ __volatile__("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(0));
}

static inline unsigned long long rdmsr(unsigned int msr) {
    unsigned int lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((unsigned long long)hi << 32) | lo;
}

static inline void wrmsr(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ __volatile__("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static int pat_available(void) {
    unsigned int a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    return (d >> 16) & 1;   /* bit 16 de EDX = PAT */
}

int pat_init(void) {
    if (!pat_available()) {
        kprintf("PAT: no soportada por la CPU\n");
        return -1;
    }

    unsigned long long pat = rdmsr(MSR_IA32_PAT);

    /* Poner la entrada 1 a WC (0x01), dejando el resto igual.
       La entrada 1 es la que usaremos para mapear el framebuffer
       con el bit PAT=1 en la PTE. */
    pat &= ~(0xFFULL << 8);
    pat |=  (0x01ULL << 8);

    /* Tambien la entrada 5 (copia de PAT1 para paginas grandes). */
    pat &= ~(0xFFULL << 40);
    pat |=  (0x01ULL << 40);

    wrmsr(MSR_IA32_PAT,
          (unsigned int)(pat & 0xFFFFFFFF),
          (unsigned int)(pat >> 32));

    kprintf("PAT: entrada 1 configurada como WC\n");
    return 0;
}

unsigned long long pat_rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__(
        "lfence\n"
        "rdtsc\n"
        : "=a"(lo), "=d"(hi) :: "memory");
    return ((unsigned long long)hi << 32) | lo;
}

} // extern "C"
