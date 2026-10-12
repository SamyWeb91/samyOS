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
#include "desktop.h"
#include "win.h"
#include "pat.h"
#include "ramdisk.h"

// Declarado en screen.cpp
void screen_set_gfx_mode(void);

// =====================================================================
// Buffer estatico del ramdisk (en .bss, no en stack)
// =====================================================================
static unsigned char ramdisk_buffer[64 * 1024] __attribute__((aligned(4096)));

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
    // PASO 2.5: PAT para write-combining del framebuffer
    // =============================================================
    pat_init();

    // =============================================================
    // PASO 3: heap
    // =============================================================
    heap_init();

    // =============================================================
    // PASO 3.5: ramdisk estatico
    // =============================================================
    {
        int rd = ramdisk_create_static((unsigned int)ramdisk_buffer,
                                       (unsigned int)sizeof(ramdisk_buffer));
        if (rd >= 0) {
            kprintf("Ramdisk: id=%d, %u bytes\n",
                    rd, (unsigned int)sizeof(ramdisk_buffer));
        } else {
            kprintf("Ramdisk: no se pudo crear\n");
        }
    }

    // =============================================================
    // PASO 4: VBE + GFX + Backbuffer
    // =============================================================
    vbe_init();

    if (vbe_available()) {
        gfx_init();

        gfx_backbuffer_init();
        gfx_clear(0x00000000);
        gfx_backbuffer_flip();

        screen_set_gfx_mode();

        kprintf("MiniOS v7.3\n");
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

    kprintf("Threads listos\n");

    // =============================================================
    // Subsistema de ventanas
    // =============================================================
    win_init();
    kprintf("Ventanas ring 3 listas\n");

    // =============================================================
    // Activar IRQs ANTES del flip final.
    // =============================================================
    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    // =============================================================
    // FLIP FINAL
    // =============================================================
    if (gfx_backbuffer_active()) {
        gfx_backbuffer_flip();
    }

    // =============================================================
    // Shell: bucle principal
    // =============================================================
    desktop_run();

    while (1) {}
}
