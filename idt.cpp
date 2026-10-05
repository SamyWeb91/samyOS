#include "idt.h"
#include "screen.h"

static IDTEntry idt[256];
static IDTPtr idt_ptr;

// =====================================================================
// Declaraciones externas de los stubs en isr.asm
// =====================================================================
extern "C" void isr_0();
extern "C" void isr_1();
extern "C" void isr_2();
extern "C" void isr_3();
extern "C" void isr_4();
extern "C" void isr_5();
extern "C" void isr_6();
extern "C" void isr_7();
extern "C" void isr_8();
extern "C" void isr_9();
extern "C" void isr_10();
extern "C" void isr_11();
extern "C" void isr_12();
extern "C" void isr_13();
extern "C" void isr_14();
extern "C" void isr_15();
extern "C" void isr_16();
extern "C" void isr_17();
extern "C" void isr_18();
extern "C" void isr_19();
extern "C" void isr_20();
extern "C" void isr_21();
extern "C" void isr_22();
extern "C" void isr_23();
extern "C" void isr_24();
extern "C" void isr_25();
extern "C" void isr_26();
extern "C" void isr_27();
extern "C" void isr_28();
extern "C" void isr_29();
extern "C" void isr_30();
extern "C" void isr_31();

extern "C" void irq_0();
extern "C" void irq_1();
extern "C" void irq_2();
extern "C" void irq_3();
extern "C" void irq_4();
extern "C" void irq_5();
extern "C" void irq_6();
extern "C" void irq_7();
extern "C" void irq_8();
extern "C" void irq_9();
extern "C" void irq_10();
extern "C" void irq_11();
extern "C" void irq_12();
extern "C" void irq_13();
extern "C" void irq_14();
extern "C" void irq_15();

// =====================================================================
// Configurar una entrada de la IDT
//
// No-static: syscall.cpp la llama para registrar int 0x80.
// =====================================================================
void idt_set_gate(int n, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[n].base_lo = base & 0xFFFF;
    idt[n].base_hi = (base >> 16) & 0xFFFF;
    idt[n].sel = sel;
    idt[n].always0 = 0;
    idt[n].flags = flags;
}

// =====================================================================
// Inicializar la IDT
// =====================================================================
void idt_init() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base = (unsigned int)&idt;

    // --- Rellenar las 256 entradas con isr_0 por defecto ---
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned int)isr_0, 0x08, 0x8E);
    }

    // --- Excepciones 0-31 (INT 0x00 - 0x1F) ---
    idt_set_gate(0,  (unsigned int)isr_0,  0x08, 0x8E);
    idt_set_gate(1,  (unsigned int)isr_1,  0x08, 0x8E);
    idt_set_gate(2,  (unsigned int)isr_2,  0x08, 0x8E);
    idt_set_gate(3,  (unsigned int)isr_3,  0x08, 0x8E);
    idt_set_gate(4,  (unsigned int)isr_4,  0x08, 0x8E);
    idt_set_gate(5,  (unsigned int)isr_5,  0x08, 0x8E);
    idt_set_gate(6,  (unsigned int)isr_6,  0x08, 0x8E);
    idt_set_gate(7,  (unsigned int)isr_7,  0x08, 0x8E);
    idt_set_gate(8,  (unsigned int)isr_8,  0x08, 0x8E);
    idt_set_gate(9,  (unsigned int)isr_9,  0x08, 0x8E);
    idt_set_gate(10, (unsigned int)isr_10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned int)isr_11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned int)isr_12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr_13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned int)isr_14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned int)isr_15, 0x08, 0x8E);
    idt_set_gate(16, (unsigned int)isr_16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned int)isr_17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned int)isr_18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned int)isr_19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned int)isr_20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned int)isr_21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned int)isr_22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned int)isr_23, 0x08, 0x8E);
    idt_set_gate(24, (unsigned int)isr_24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned int)isr_25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned int)isr_26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned int)isr_27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned int)isr_28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned int)isr_29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned int)isr_30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned int)isr_31, 0x08, 0x8E);

    // --- IRQs 0-15 (INT 0x20 - 0x2F, tras remapear el PIC) ---
    idt_set_gate(32, (unsigned int)irq_0,  0x08, 0x8E);
    idt_set_gate(33, (unsigned int)irq_1,  0x08, 0x8E);
    idt_set_gate(34, (unsigned int)irq_2,  0x08, 0x8E);
    idt_set_gate(35, (unsigned int)irq_3,  0x08, 0x8E);
    idt_set_gate(36, (unsigned int)irq_4,  0x08, 0x8E);
    idt_set_gate(37, (unsigned int)irq_5,  0x08, 0x8E);
    idt_set_gate(38, (unsigned int)irq_6,  0x08, 0x8E);
    idt_set_gate(39, (unsigned int)irq_7,  0x08, 0x8E);
    idt_set_gate(40, (unsigned int)irq_8,  0x08, 0x8E);
    idt_set_gate(41, (unsigned int)irq_9,  0x08, 0x8E);
    idt_set_gate(42, (unsigned int)irq_10, 0x08, 0x8E);
    idt_set_gate(43, (unsigned int)irq_11, 0x08, 0x8E);
    idt_set_gate(44, (unsigned int)irq_12, 0x08, 0x8E);
    idt_set_gate(45, (unsigned int)irq_13, 0x08, 0x8E);
    idt_set_gate(46, (unsigned int)irq_14, 0x08, 0x8E);
    idt_set_gate(47, (unsigned int)irq_15, 0x08, 0x8E);

    // --- Cargar la IDT ---
    asm volatile("lidt %0" : : "m"(idt_ptr));
}
