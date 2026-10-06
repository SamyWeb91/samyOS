#pragma once

#include <stdint.h>

struct TSS {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

#ifdef __cplusplus
extern "C" {
#endif

void tss_init(void);
void tss_set_kernel_stack(uint32_t esp0);
uint32_t tss_get_esp0(void);
uint32_t tss_get_ss0(void);
uint32_t tss_get_base(void);
uint32_t tss_get_limit(void);

#ifdef __cplusplus
}
#endif
