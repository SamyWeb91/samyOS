// hola_mundo.cpp (Versión simple y segura)
#define SYS_EXIT          0
#define SYS_DRAW_TEXT     13
#define SYS_CREATE_WINDOW 14

static inline void sys_draw_text(int x, int y, unsigned int color, const char* str) {
    asm volatile("int $0x80" : : "a"(SYS_DRAW_TEXT), "b"(x), "c"(y), "d"(color), "S"(str) : "memory");
}

static inline int sys_create_window(int x, int y, int w, int h, const char* title) {
    int wid;
    asm volatile("int $0x80" : "=a"(wid) : "a"(SYS_CREATE_WINDOW), "b"(x), "c"(y), "d"(w), "S"(h), "D"(title) : "memory");
    return wid;
}

static inline void sys_exit(int code) {
    asm volatile("int $0x80" : : "a"(SYS_EXIT), "b"(code) : "memory");
}

extern "C" void _start() {
    // Posición fija de la ventana
    int wx = 200;
    int wy = 150;

    int wid = sys_create_window(wx, wy, 400, 300, "Mi Primera App");

    if (wid >= 0) {
        // Dibujamos sumando la posición de la ventana (150+50 = 200)
        sys_draw_text(wx + 50, wy + 50, 0x00000000, "Hola Mundo desde Ring 3!");
        sys_draw_text(wx + 50, wy + 70, 0x00000000, "Ventana creada por una Syscall.");
        sys_draw_text(wx + 50, wy + 100, 0x0000FF00, "Texto en verde relativo a la ventana.");
    }

    sys_exit(0);
    while(1) {}
}
