/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 * (licencia GPL...)
 */

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
#include "process.h"   // <--- NUEVO

void screen_set_gfx_mode(void);
extern "C" uint32_t fat32_scan(void);
extern "C" int fat32_mount(uint32_t part_lba);

static unsigned char ramdisk_buffer[64 * 1024] __attribute__((aligned(4096)));

extern "C" void kernel_main(void) {
    idt_init();
    gdt_init();
    tss_init();
    pic_remap();
    pit_init(100);
    keyboard_init();

    paging_init();
    paging_enable_now();
    pat_init();
    heap_init();

    {
        int rd = ramdisk_create_static((unsigned int)ramdisk_buffer, sizeof(ramdisk_buffer));
        if (rd >= 0) kprintf("Ramdisk: id=%d\n", rd);
    }

    vbe_init();
    if (vbe_available()) {
        gfx_init();
        gfx_backbuffer_init();
        gfx_clear(0x00000000);
        gfx_backbuffer_flip();
        screen_set_gfx_mode();
        kprintf("SamyOS v0.10.7 \n=================\n\n"); // Actualicé la versión por si acaso
    } else {
        kprintf("VBE no disponible\n");
    }

    kprintf("IDT, GDT, TSS, PIC, PIT, Teclado, Paging, Heap OK\n");

    // Iniciar AHCI y FAT32
    kprintf("Iniciando AHCI...\n");
    if (ahci_init()) {
        kprintf("AHCI OK\n");
    } else {
        kprintf("AHCI: no disponible\n");
    }

    syscall_init();
    user_init();

    kprintf("Iniciando FAT32...\n");
    uint32_t fat_lba = fat32_scan();
    if (fat_lba && fat32_mount(fat_lba)) {
        kprintf("FAT32: Montada en LBA %u\n", fat_lba);
    } else {
        kprintf("FAT32: Error\n");
    }

    thread_init();
    process_init(); // <--- NUEVO: Inicializa la tabla de procesos
    win_init();

    asm volatile("sti");

    if (gfx_backbuffer_active()) {
        gfx_backbuffer_flip();
    }

    // =============================================================
    // INICIAMOS EL ESCRITORIO
    // =============================================================
    kprintf("\nIniciando escritorio...\n");
    desktop_run();

    // Si el escritorio termina (no debería), nos quedamos aqui.
    while (1) {}
}
