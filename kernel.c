#include "screen.h"
#include "idt.h"
#include "gdt.h"
#include "tss.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"
#include "rtc.h"
#include "paging.h"
#include "fs.h"
#include "ahci.h"
#include "thread.h"
#include "syscall.h"
#include "user.h"
#include "vbe.h"
#include "gfx.h"

// Declarado en screen.cpp
 void screen_set_gfx_mode(void);

// =====================================================================
// Workers
// =====================================================================
void worker1(void) {
    volatile unsigned int x = 0;
    while (1) { x++; }
}

void worker2(void) {
    volatile unsigned int y = 0;
    while (1) { y++; }
}

// =====================================================================
// kernel_main
// =====================================================================
extern "C" void kernel_main(void) {
    // =============================================================
    // PASO 1: init básico (sin prints — aún no hay salida visible)
    // =============================================================
    idt_init();
    gdt_init();
    tss_init();
    pic_remap();
    pit_init(100);
    keyboard_init();
    heap_init();
    paging_init();
    paging_enable_now();

    // =============================================================
    // PASO 2: VBE + GFX
    //
    // El bootloader cambió a modo gráfico. Hay que:
    //   1. Leer info VBE de 0x6000.
    //   2. Mapear el framebuffer (gfx_init).
    //   3. Activar el modo gráfico en screen.cpp (screen_set_gfx_mode).
    //
    // A partir de aquí, kprintf dibuja en el framebuffer.
    // =============================================================
    vbe_init();

    if (vbe_available()) {
        gfx_init();
        screen_set_gfx_mode();     // ← CLAVE: activa el modo gráfico en screen.cpp

        // Ahora ya se ve el output
        kprintf("MiniOS v5.0 + VBE\n");
        kprintf("=================\n\n");
        kprintf("VBE: %ux%u %ubpp\n",
                vbe_width(), vbe_height(), vbe_bpp());
        kprintf("FB:  %x  pitch=%u bytes\n",
                vbe_fb_addr(), vbe_pitch());
        kprintf("\n");
    } else {
        // VBE no disponible: el bootloader cambió a gráfico pero no hay info.
        // No se verá nada. Pero el kernel sigue.
        // (No podemos imprimir nada visible.)
    }

    // =============================================================
    // PASO 3: resto del init (ya visible)
    // =============================================================
    kprintf("IDT cargada\n");
    kprintf("GDT recargada\n");
    kprintf("TSS cargado\n");
    kprintf("PIC remapeado\n");
    kprintf("PIT a 100 Hz\n");
    kprintf("Teclado listo\n");
    kprintf("Heap inicializado\n");
    kprintf("Paginacion activada\n");

    paging_map_range(0xFE000000, 0xFE000000, 32768, PTE_PRESENT | PTE_RW);
    kprintf("MMIO mapeado (0xFE000000 - 0xFFFFFFFF)\n");

    kprintf("\n");
    syscall_init();

    user_init();

    kprintf("\n");
    if (ahci_init()) {
        kprintf("AHCI listo\n");
    } else {
        kprintf("AHCI no disponible\n");
    }

    fs_init();

    kprintf("\n");
    thread_init();

    thread_create("worker1", worker1);
    thread_create("worker2", worker2);

    kprintf("Threads listos\n");

    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    shell_run();

    while (1) {}
}
