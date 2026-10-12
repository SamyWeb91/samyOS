// keyboard.cpp
#include "keyboard.h"
#include "io.h"
#include "thread.h"

/* =====================================================================
 * Buffer circular de caracteres
 * ===================================================================== */
#define KBUF_SIZE 64
static volatile char kbuf[KBUF_SIZE];
static volatile int kbuf_head = 0;
static volatile int kbuf_tail = 0;

/* =====================================================================
 * Estado de los modificadores
 * ===================================================================== */
static volatile int k_shift = 0;
static volatile int k_ctrl  = 0;
static volatile int k_alt   = 0;

/* Prefijo E0 (teclas extendidas: flechas, ctrl derecho, etc.) */
static volatile int kbd_extended = 0;

/* =====================================================================
 * Tablas de scancode -> ASCII (Set 1)
 *
 * Verificadas: 128 entradas cada una. El comentario /* XX *​/ indica
 * el indice (en hex) de la siguiente entrada.
 * ===================================================================== */
static const char kbd_us[128] = {
    /* 00 */ 0, 27,
    /* 02 */ '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    /* 0F */ '\t',
    /* 10 */ 'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    /* 1D */ 0,
    /* 1E */ 'a','s','d','f','g','h','j','k','l',';','\'','`',
    /* 2A */ 0,
    /* 2B */ '\\','z','x','c','v','b','n','m',',','.','/',
    /* 36 */ 0,
    /* 37 */ '*',
    /* 38 */ 0,
    /* 39 */ ' ',
    /* 3A */ 0,
    /* 3B */ 0,0,0,0,0,0,0,0,0,0,   /* F1-F10 */
    /* 45 */ 0,                     /* NumLock */
    /* 46 */ 0,                     /* ScrollLock */
    /* 47 */ 0,                     /* Home */
    /* 48 */ 0,                     /* Up */
    /* 49 */ 0,                     /* PageUp */
    /* 4A */ '-',                   /* Num- */
    /* 4B */ 0,                     /* Left */
    /* 4C */ 0,                     /* Center */
    /* 4D */ 0,                     /* Right */
    /* 4E */ '+',                   /* Num+ */
    /* 4F */ 0,                     /* End */
    /* 50 */ 0,                     /* Down */
    /* 51 */ 0,                     /* PageDown */
    /* 52 */ 0,                     /* Insert */
    /* 53 */ 0,                     /* Delete */
    /* 54 */ 0,0,0,
    /* 57 */ 0,                     /* F11 */
    /* 58 */ 0,                     /* F12 */
    /* 59 */ 0,0,0,0,0,0,0,
    /* 60 */ 0,0,0,0,0,0,0,0,
    /* 68 */ 0,0,0,0,0,0,0,0,
    /* 70 */ 0,0,0,0,0,0,0,0,
    /* 78 */ 0,0,0,0,0,0,0,0
};

static const char kbd_us_shift[128] = {
    /* 00 */ 0, 27,
    /* 02 */ '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    /* 0F */ '\t',
    /* 10 */ 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    /* 1D */ 0,
    /* 1E */ 'A','S','D','F','G','H','J','K','L',':','"','~',
    /* 2A */ 0,
    /* 2B */ '|','Z','X','C','V','B','N','M','<','>','?',
    /* 36 */ 0,
    /* 37 */ '*',
    /* 38 */ 0,
    /* 39 */ ' ',
    /* 3A */ 0,
    /* 3B */ 0,0,0,0,0,0,0,0,0,0,
    /* 45 */ 0,
    /* 46 */ 0,
    /* 47 */ 0,
    /* 48 */ 0,
    /* 49 */ 0,
    /* 4A */ '-',
    /* 4B */ 0,
    /* 4C */ 0,
    /* 4D */ 0,
    /* 4E */ '+',
    /* 4F */ 0,
    /* 50 */ 0,
    /* 51 */ 0,
    /* 52 */ 0,
    /* 53 */ 0,
    /* 54 */ 0,0,0,
    /* 57 */ 0,
    /* 58 */ 0,
    /* 59 */ 0,0,0,0,0,0,0,
    /* 60 */ 0,0,0,0,0,0,0,0,
    /* 68 */ 0,0,0,0,0,0,0,0,
    /* 70 */ 0,0,0,0,0,0,0,0,
    /* 78 */ 0,0,0,0,0,0,0,0
};

/* Verificacion en tiempo de compilacion: si el tamano no es 128, falla */
typedef char check_kbd_us_size[(sizeof(kbd_us) == 128) ? 1 : -1];
typedef char check_kbd_us_shift_size[(sizeof(kbd_us_shift) == 128) ? 1 : -1];

/* =====================================================================
 * Helpers
 * ===================================================================== */
static void kbuf_push(char c) {
    int next = (kbuf_head + 1) % KBUF_SIZE;
    if (next != kbuf_tail) {
        kbuf[kbuf_head] = c;
        kbuf_head = next;
    }
}

/* Ctrl: 'a'..'z' -> 1..26 */
static char apply_ctrl(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 1);
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 1);
    return c;
}

/* =====================================================================
 * Init
 * ===================================================================== */
void keyboard_init() {
    while (inb(0x64) & 1) inb(0x60);
    kbuf_head = 0;
    kbuf_tail = 0;
    k_shift = 0;
    k_ctrl  = 0;
    k_alt   = 0;
    kbd_extended = 0;
}

/* =====================================================================
 * Handler de IRQ1
 * ===================================================================== */
extern "C" void keyboard_handler() {
    unsigned char sc = inb(0x60);

    if (sc == 0xE0) {
        kbd_extended = 1;
        return;
    }
    if (sc == 0xE1) {
        kbd_extended = 0;
        return;
    }

    int is_release = (sc & 0x80) != 0;
    unsigned char code = sc & 0x7F;

    /* Modificadores sin E0 */
    if (!kbd_extended) {
        if (code == 0x2A || code == 0x36) { k_shift = !is_release; return; }
        if (code == 0x1D)                 { k_ctrl  = !is_release; return; }
        if (code == 0x38)                 { k_alt   = !is_release; return; }
    } else {
        /* Modificadores con E0 */
        if (code == 0x1D) { k_ctrl = !is_release; kbd_extended = 0; return; }
        if (code == 0x38) { k_alt  = !is_release; kbd_extended = 0; return; }
    }

    /* Solo nos interesan las pulsaciones de teclas normales */
    if (is_release) {
        kbd_extended = 0;
        return;
    }

    /* Teclas extendidas: ignoradas por ahora */
    if (kbd_extended) {
        kbd_extended = 0;
        return;
    }

    /* Tecla normal */
    char c = kbd_us[code];
    if (c == 0) return;

    if (k_ctrl) {
        c = apply_ctrl(c);
        kbuf_push(c);
        return;
    }

    if (k_shift) {
        char cs = kbd_us_shift[code];
        if (cs != 0) c = cs;
    }

    kbuf_push(c);
}

/* =====================================================================
 * API publica
 * ===================================================================== */
char keyboard_getchar() {
    if (kbuf_head == kbuf_tail) return 0;
    char c = kbuf[kbuf_tail];
    kbuf_tail = (kbuf_tail + 1) % KBUF_SIZE;
    return c;
}
