// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
static volatile int k_caps  = 0; // <-- Nuevo: estado de Caps Lock

/* Prefijo E0 (teclas extendidas: flechas, ctrl derecho, etc.) */
static volatile int kbd_extended = 0;

/* =====================================================================
 * Tablas de scancode -> ASCII (Set 1)
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
    k_caps  = 0;
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
        if (code == 0x2A || code == 0x36) { k_shift = !is_release; return; } // Shift
        if (code == 0x1D)                 { k_ctrl  = !is_release; return; } // Ctrl
        if (code == 0x38)                 { k_alt   = !is_release; return; } // Alt
        if (code == 0x3A && !is_release)  { k_caps  = !k_caps; return; }     // Caps Lock
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

    // Aplicar Ctrl
    if (k_ctrl) {
        c = apply_ctrl(c);
        kbuf_push(c);
        return;
    }

    // Lógica combinada de Shift y Caps Lock
    // Si es una letra (a-z):
    if (c >= 'a' && c <= 'z') {
        // Si (Shift XOR Caps Lock) es verdadero, la letra es mayúscula
        if (k_shift ^ k_caps) {
            c = kbd_us_shift[code]; // Buscar la versión mayúscula en la tabla shift
        }
    } 
    // Si NO es una letra (es un número o símbolo):
    else {
        // Solo nos importa si Shift está presionado
        if (k_shift) {
            char cs = kbd_us_shift[code];
            if (cs != 0) c = cs;
        }
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
