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

// Necesario para registrar int 0x80 desde syscall.cpp
void idt_set_gate(int n, unsigned int base, unsigned short sel, unsigned char flags);
