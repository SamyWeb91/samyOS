#include "pit.h"
#include "io.h"

#define PIT_CHANNEL0  0x40
#define PIT_COMMAND   0x43
#define PIT_BASE_FREQ 1193180

static volatile unsigned int ticks = 0;

void pit_init(unsigned int freq) {
    unsigned int divisor = PIT_BASE_FREQ / freq;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (unsigned char)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (unsigned char)((divisor >> 8) & 0xFF));
}

unsigned int pit_get_ticks() {
    return ticks;
}

extern "C" void pit_tick() {
    ticks++;
}
