#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gdt_init();
void gdt_set_tss(uint32_t base, uint32_t limit);

#ifdef __cplusplus
}
#endif
