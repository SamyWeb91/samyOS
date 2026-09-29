#include "gdt.h"
#include "screen.h"

static uint64_t gdt[6];

struct GDTDescriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static GDTDescriptor gdt_descriptor;

extern "C" void gdt_load(uint32_t gdt_ptr);
extern "C" void gdt_reload_segments();

static uint64_t make_gdt_entry(uint32_t base, uint32_t limit,
                                uint8_t access, uint8_t gran) {
    uint64_t entry = 0;
    entry |= (uint64_t)(limit & 0xFFFF);              // limit 0:15
    entry |= (uint64_t)(base & 0xFFFF) << 16;         // base 0:15
    entry |= (uint64_t)((base >> 16) & 0xFF) << 32;   // base 16:23
    entry |= (uint64_t)access << 40;                  // access byte
    entry |= (uint64_t)((limit >> 16) & 0x0F) << 48;  // limit 16:19
    entry |= (uint64_t)(gran & 0xF0) << 48;           // flags en byte 6
    entry |= (uint64_t)((base >> 24) & 0xFF) << 56;   // base 24:31
    return entry;
}

extern "C" void gdt_init() {
    gdt[0] = 0;
    gdt[1] = make_gdt_entry(0, 0xFFFFF, 0x9A, 0xCF);  // codigo kernel
    gdt[2] = make_gdt_entry(0, 0xFFFFF, 0x92, 0xCF);  // datos kernel
    gdt[3] = make_gdt_entry(0, 0xFFFFF, 0xFA, 0xCF);  // codigo usuario
    gdt[4] = make_gdt_entry(0, 0xFFFFF, 0xF2, 0xCF);  // datos usuario
    gdt[5] = 0;                                        // TSS (vacio)

    gdt_descriptor.limit = sizeof(gdt) - 1;
    gdt_descriptor.base  = (uint32_t)gdt;

    gdt_load((uint32_t)&gdt_descriptor);
    gdt_reload_segments();
}

extern "C" void gdt_set_tss(uint32_t base, uint32_t limit) {
    gdt[5] = make_gdt_entry(base, limit, 0x89, 0x00);
}
