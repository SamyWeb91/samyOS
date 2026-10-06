#include "keyboard.h"
#include "io.h"
#include "thread.h"

// Buffer circular
#define KBUF_SIZE 64
static volatile char kbuf[KBUF_SIZE];
static volatile int kbuf_head = 0;
static volatile int kbuf_tail = 0;

static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' ',
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

void keyboard_init() {
    while (inb(0x64) & 1) inb(0x60);
    kbuf_head = 0;
    kbuf_tail = 0;
}

extern "C" void keyboard_handler() {
    unsigned char scancode = inb(0x60);
    if (scancode & 0x80) return;
    char c = scancode_to_ascii[scancode];
    if (c == 0) return;

    int next = (kbuf_head + 1) % KBUF_SIZE;
    if (next != kbuf_tail) {
        kbuf[kbuf_head] = c;
        kbuf_head = next;
    }
}

char keyboard_getchar() {
    if (kbuf_head == kbuf_tail) return 0;
    char c = kbuf[kbuf_tail];
    kbuf_tail = (kbuf_tail + 1) % KBUF_SIZE;
    return c;
}
