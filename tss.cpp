#include "tss.h"
#include "gdt.h"
#include "screen.h"

static TSSEntry tss_entry;

extern "C" void tss_flush(uint16_t tss_selector);

extern "C" void tss_init() {
    unsigned char *p = (unsigned char*)&tss_entry;
    for (unsigned int i = 0; i < sizeof(TSSEntry); i++) {
        p[i] = 0;
    }

    tss_entry.ss0  = 0x10;
    tss_entry.esp0 = 0x90000;
    tss_entry.iomap_base = sizeof(TSSEntry);

    gdt_set_tss((uint32_t)&tss_entry, sizeof(TSSEntry) - 1);

    tss_flush(0x28);

    kprintf("TSS cargado (base=0x%x, esp0=0x%x)\n",
            (uint32_t)&tss_entry, tss_entry.esp0);
}

extern "C" void tss_set_kernel_stack(uint32_t esp0) {
    tss_entry.esp0 = esp0;
}
