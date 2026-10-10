#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Arranca el escritorio. Bloquea (bucle principal).
void desktop_run(void);

// Redirección del terminal gráfico (usadas por screen.cpp)
void desktop_terminal_clear(void);
void desktop_terminal_putchar(char c);

#ifdef __cplusplus
}
#endif
