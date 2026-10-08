// kernel.c
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
// Worker threads (demo)
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
    // PASO 1: init básico
    // =============================================================
    idt_init();
    gdt_init();
    tss_init();
    pic_remap();
    pit_init(100);
    keyboard_init();

    // =============================================================
    // PASO 2: paging ANTES que el heap
    // =============================================================
    paging_init();
    paging_enable_now();

    // =============================================================
    // PASO 3: heap
    // =============================================================
    heap_init();

    // =============================================================
    // PASO 4: VBE + GFX + Backbuffer
    // =============================================================
    vbe_init();

    if (vbe_available()) {
        gfx_init();

        // Inicializar el backbuffer ANTES de cualquier kprintf.
        gfx_backbuffer_init();

        // Limpiar el backbuffer a negro
        gfx_clear(0x00000000);

        // Volcar el backbuffer limpio al framebuffer de una vez.
        // Esto limpia TODA la basura inicial (incluido el padding).
        gfx_backbuffer_flip();

        screen_set_gfx_mode();

        kprintf("MiniOS v7.0\n");
        kprintf("=================\n\n");
        kprintf("VBE: %ux%u %ubpp\n",
                vbe_width(), vbe_height(), vbe_bpp());
        kprintf("FB:  %x  pitch=%u bytes\n",
                vbe_fb_addr(), vbe_pitch());
        kprintf("\n");
    } else {
        kprintf("VBE no disponible, sigo en modo texto\n");
    }

    // =============================================================
    // PASO 5: resto del init
    // =============================================================
    kprintf("IDT cargada\n");
    kprintf("GDT recargada\n");
    kprintf("TSS cargado\n");
    kprintf("PIC remapeado\n");
    kprintf("PIT a 100 Hz\n");
    kprintf("Teclado listo\n");
    kprintf("Paging activado\n");
    kprintf("Heap inicializado\n");

    kprintf("MMIO: no mapeado (modo seguro, sin AHCI)\n");

    kprintf("\n");
    syscall_init();
    user_init();

    kprintf("\n");
    kprintf("[MODO SEGURO] AHCI y FS deshabilitados (no se toca el disco)\n");

    kprintf("\n");
    thread_init();

    thread_create("worker1", worker1);
    thread_create("worker2", worker2);

    kprintf("Threads listos\n");

    // =============================================================
    // Activar IRQs ANTES del flip final.
    // =============================================================
    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    // =============================================================
    // FLIP FINAL: volcar todo el backbuffer al framebuffer.
    // Se hace DESPUES de sti, para que las IRQs no se queden
    // bloqueadas mientras dura el flip.
    // =============================================================
    if (gfx_backbuffer_active()) {
        gfx_backbuffer_flip();
    }

    // =============================================================
    // Shell: bucle principal
    // =============================================================
    shell_run();

    while (1) {}
}
