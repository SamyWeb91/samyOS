#pragma once

void keyboard_init();
extern "C" void keyboard_handler();
char keyboard_getchar();   // devuelve 0 si no hay tecla
