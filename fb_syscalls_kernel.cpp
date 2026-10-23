// fb_syscalls_kernel.cpp
// Handlers de las syscalls del framebuffer y de input.
// Regístralos desde syscall_init().
#include "syscall_fb.h"
#include "gfx.h"
#include "paging.h"
#include "mouse.h"
#include "keyboard.h"
#include "pit.h"
#include "thread.h"

// Estos dos los debes tener ya en tu capa de syscalls:
extern "C" int  sys_current_pid(void);
extern "C" int  sys_map_phys_in_user(unsigned int pid,
                                     unsigned int phys,
                                     unsigned int len_bytes,
                                     unsigned int user_va,
                                     unsigned int flags);

// --- SYS_FB_INFO ---------------------------------------------------
// Devuelve info basica; el mapeo del VA se hace en SYS_FB_MAP.
extern "C" int k_sys_fb_info(struct fb_info *out) {
    if (!out) return -1;
    out->va    = 0;
    out->w     = gfx_backbuffer_width();
    out->h     = gfx_backbuffer_height();
    out->pitch = gfx_backbuffer_width();
    return 0;
}

// --- SYS_FB_MAP ----------------------------------------------------
// Mapea el backbuffer del kernel en el espacio del proceso llamante.
// El VA destino lo elige el kernel (devuelve el elegido en *out_va).
extern "C" int k_sys_fb_map(unsigned int *out_va) {
    uint32_t *bb = gfx_backbuffer_va();
    if (!bb) return -1;

    int w = gfx_backbuffer_width();
    int h = gfx_backbuffer_height();
    unsigned int bytes = (unsigned int)w * (unsigned int)h * 4u;

    // Aqui necesitas que tu paging tenga una primitiva para mapear
    // paginas fisicas ya existentes en el address space del proceso
    // actual, devolviendo el VA elegido. Ejemplo de firma:
    //
    //   unsigned int paging_map_shared_in_current(unsigned int phys,
    //                                             unsigned int len,
    //                                             unsigned int pte_flags);
    //
    // Debe ser idempotente: si ya estaba mapeado, devuelve el mismo VA.
    extern unsigned int paging_map_shared_in_current(unsigned int phys,
                                                     unsigned int len,
                                                     unsigned int pte_flags);

    unsigned int va = paging_map_shared_in_current(
        (unsigned int)bb, bytes,
        PTE_PRESENT | PTE_RW | PTE_USER);

    if (!va) return -1;
    *out_va = va;
    return 0;
}

// --- SYS_FB_FLIP_RECT ---------------------------------------------
extern "C" int k_sys_fb_flip_rect(int x, int y, int w, int h) {
    gfx_backbuffer_flip_region(x, y, w, h);
    return 0;
}

// --- SYS_KEY_POLL --------------------------------------------------
// Devuelve -1 si no hay tecla, o el ASCII si lo hay.
extern "C" int k_sys_key_poll(void) {
    char c = keyboard_getchar();
    return c ? (int)(unsigned char)c : -1;
}

// --- SYS_MOUSE_POLL ------------------------------------------------
extern "C" int k_sys_mouse_poll(struct mouse_event *ev) {
    if (!ev) return -1;
    // Ajusta a la API real de tu mouse.cpp:
    //   mouse_get_delta(&dx,&dy), mouse_get_buttons(), mouse_get_pos()
    extern int mouse_get_delta(int *dx, int *dy);
    extern int mouse_get_buttons(void);
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);

    int dx = 0, dy = 0;
    mouse_get_delta(&dx, &dy);

    ev->dx      = dx;
    ev->dy      = dy;
    ev->buttons = mouse_get_buttons();
    ev->x       = mouse_get_x();
    ev->y       = mouse_get_y();
    return 0;
}

// --- SYS_TICKS -----------------------------------------------------
extern "C" unsigned int k_sys_ticks(void) {
    return pit_get_ticks();
}

// --- SYS_YIELD -----------------------------------------------------
extern "C" void k_sys_yield(void) {
    thread_yield();
}
