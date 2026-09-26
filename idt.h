#pragma once

struct IDTEntry {
    unsigned short base_lo;
    unsigned short sel;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_hi;
} __attribute__((packed));

struct IDTPtr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

void idt_init();
