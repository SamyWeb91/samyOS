// test_extra.cpp
// Archivo de prueba para verificar que se pueden anadir .o nuevos
// sin romper el desktop.

#include <stdint.h>

extern "C" int test_extra_value(void) {
    return 42;
}

extern "C" const char *test_extra_name(void) {
    return "test_extra";
}

// Un array de 16 KB en .bss para simular el "peso" de un driver real
static uint8_t test_extra_buffer[16384];

extern "C" int test_extra_sum(void) {
    int s = 0;
    for (int i = 0; i < 16384; i++) s += test_extra_buffer[i];
    return s;
}
