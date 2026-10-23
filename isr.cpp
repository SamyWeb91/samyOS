#include "isr.h"
#include "screen.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "thread.h"
#include "mouse.h"

// =====================================================================
// Dump de registros para diagnóstico
// =====================================================================
static void dump_regs(Regs *r) {
    kprintf("  EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
            r->eax, r->ebx, r->ecx, r->edx);
    // FIX: el campo del struct se llama esp_dummy (no esp_orig).
    // Corresponde al ESP antes del pusha (no es el ESP del contexto
    // interrumpido; ese está en r->useresp si hubo cambio de ring).
    kprintf("  ESI=%08x EDI=%08x EBP=%08x ESP_dummy=%08x\n",
            r->esi, r->edi, r->ebp, r->esp_dummy);
    kprintf("  EIP=%08x CS=%04x EFLAGS=%08x\n",
            r->eip, r->cs & 0xFFFF, r->eflags);
    kprintf("  DS=%04x ES=%04x FS=%04x GS=%04x SS=%04x\n",
            r->ds & 0xFFFF, r->es & 0xFFFF,
            r->fs & 0xFFFF, r->gs & 0xFFFF,
            r->ss & 0xFFFF);
    // Si hubo cambio de ring, mostrar la pila del usuario también
    if ((r->cs & 3) == 3) {
        kprintf("  [ring3] useresp=%08x ss=%04x\n",
                r->useresp, r->ss & 0xFFFF);
    }
}

extern "C" void isr_handler(Regs *r) {
    // =================================================================
    // Excepciones (0-31)
    // =================================================================
    if (r->int_no < 32) {
        // --- PAGE FAULT (vector 14) ---
        if (r->int_no == 14) {
            unsigned int cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));

            kprintf("\n[PAGE FAULT] EIP=0x%x CR2=0x%x ERR=0x%x\n",
                    r->eip, cr2, r->err_code);

            // FIX: decodificar el error code del #PF para diagnóstico
            kprintf("  P=%d W=%d U=%d RSVD=%d ID=%d\n",
                    (r->err_code & 1) ? 1 : 0,   // P: 1=protección, 0=no presente
                    (r->err_code & 2) ? 1 : 0,   // W: 1=escritura, 0=lectura
                    (r->err_code & 4) ? 1 : 0,   // U: 1=user, 0=kernel
                    (r->err_code & 8) ? 1 : 0,   // RSVD: bit reservado
                    (r->err_code & 16) ? 1 : 0); // ID: instruction fetch

            dump_regs(r);

            // FIX: NO usar while(1){cli;hlt;} porque IF=0 impide
            // que la CPU despierte. Usar cli;hlt;cli;hlt;... en bucle
            // infinito. La CPU queda detenida y QEMU puede pausar
            // limpiamente con Ctrl+C o el monitor.
            // Si prefieres que el sistema siga vivo, quita este bucle
            // y simplemente retorna (pero el proceso de usuario
            // seguirá ejecutando la instrucción faulting en bucle).
            while (1) {
                asm volatile("cli; hlt");
            }
        }

        // --- OTRAS EXCEPCIONES ---
        // FIX: para #GP (13) y #DF (8) imprimir más contexto.
        kprintf("\nEXCEPCION %d (err=0x%x) en EIP=0x%x\n",
                r->int_no, r->err_code, r->eip);
        dump_regs(r);

        // FIX: igual que arriba, cli;hlt en bucle en vez de while(1){}
        // (while(1){} deja la CPU corriendo al 100% y QEMU no pausa bien).
        while (1) {
            asm volatile("cli; hlt");
        }
    }

    // =================================================================
    // IRQs (32-47)
    // =================================================================
    if (r->int_no >= 32 && r->int_no <= 47) {
        unsigned int irq = r->int_no - 32;

        // FIX: IRQ 7 y 15 son espurias. El PIC las reporta pero no
        // hay dispositivo real detrás. Se debe enviar EOI al PIC
        // maestro (para IRQ7) o esclavo (para IRQ15) y retornar.
        // Si no se maneja, el PIC se queda bloqueado.
        if (irq == 7 || irq == 15) {
            pic_send_eoi(irq);
            return;
        }

        if (irq == 0) {
            // Timer: EOI ANTES de conmutar.
            pit_tick();
            pic_send_eoi(irq);
            schedule();
            return;
        }

        if (irq == 1) {
            keyboard_handler();
        }
        else if (irq == 12) {
            mouse_handler();
        }

        pic_send_eoi(irq);
    }
}
