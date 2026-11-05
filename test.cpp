// test.cpp

#define SYS_EXIT          0
#define SYS_DRAW_TEXT     13
#define SYS_CREATE_WINDOW 14
#define SYS_YIELD         99

// Usamos static inline para asegurar que el compilador las incruste (inline) 
// y no intente buscarlas en el enlazador.
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

static inline void sys_yield() {
    asm volatile("int $0x80" : : "a"(SYS_YIELD) : "memory");
}

extern "C" void _start() {
    // 1. Pedir al kernel que cree una ventana real
    int wid = sys_create_window(200, 150, 400, 300, "Test Multitarea");

    if (wid >= 0) {
        // 2. Dibujar texto dentro de esa ventana
        sys_draw_text(250, 250, 0x00000000, "Hola Mundo desde Ring 3!");
        sys_draw_text(250, 270, 0x00000000, "El escritorio no se ha congelado.");

        // 3. Hacemos un bucle controlado para que la ventana se quede en pantalla
        // y podamos mover el ratón para demostrar que hay multitarea.
        for (volatile int i = 0; i < 5; i++) {
            // Hacemos un montón de yields para que el ratón vaya fluido
            for (volatile int j = 0; j < 100000; j++) {
                sys_yield();
            }
        }
    }

    // 4. Salir (esto cierra el programa y libera la memoria)
    sys_exit(0);

    while(1) {}
}
