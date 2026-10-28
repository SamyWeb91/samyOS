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

void screen_set_gfx_mode(void);
extern "C" uint32_t fat32_scan(void);
extern "C" int fat32_mount(uint32_t part_lba);

// Llamamos al CARGADOR (ring3_test_run)
extern "C" void ring3_test_run(void);

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
        kprintf("SamyOS v0.10.1 \n=================\n\n");
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
    win_init();

    asm volatile("sti");

    if (gfx_backbuffer_active()) {
        gfx_backbuffer_flip();
    }

    // =============================================================
    // AQUI LLAMAMOS AL CARGADOR (que hará el salto al binario en Ring 3)
    // =============================================================
    kprintf("\n!!! LLAMANDO AL CARGADOR RING 3 !!!\n");
    ring3_test_run();

    // Si el binario termina y vuelve, caemos al escritorio
    kprintf("\nVolviendo al escritorio...\n");
    desktop_run();

    while (1) {}
}
