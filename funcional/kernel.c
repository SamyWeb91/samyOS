#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "shell.h"
#include "heap.h"
#include "rtc.h"
#include "paging.h"

extern "C" void kernel_main() {
    screen_clear();
    kprintf("MiniOS v1.0\n");
    kprintf("===========\n\n");

    idt_init();
    kprintf("IDT cargada\n");

    pic_remap();
    kprintf("PIC remapeado\n");

    pit_init(100);
    kprintf("PIT a 100 Hz\n");

    keyboard_init();
    kprintf("Teclado listo\n");

    heap_init();
    kprintf("Heap inicializado\n");

    // --- Diagnostico de paginacion ---
    kprintf("\nInicializando paginacion (sin activar)...\n");
    paging_init();

    unsigned int pd = paging_get_pd_addr();
    unsigned int pt = paging_get_pt0_addr();
    kprintf("Page Directory: %x (alineado: %s)\n", pd, (pd & 0xFFF) == 0 ? "si" : "NO");
    kprintf("Page Table 0:   %x (alineado: %s)\n", pt, (pt & 0xFFF) == 0 ? "si" : "NO");

    // Verificar que la PT apunta a la PD
    volatile unsigned int *pd_ptr = (volatile unsigned int*)pd;
    kprintf("PD[0]:          %x\n", pd_ptr[0]);
    kprintf("Esperado:       %x\n", pt | 0x3);

    // Activar paginacion
    kprintf("\nActivando paginacion...\n");
    paging_enable_now();
    kprintf("Paginacion activada!\n");

    asm volatile("sti");

    kprintf("\nEscribe 'help' para ver los comandos.\n\n");

    shell_run();

    while (1) {}
}
