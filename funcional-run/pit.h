#pragma once

void pit_init(unsigned int freq);
unsigned int pit_get_ticks();
extern "C" void pit_tick();
