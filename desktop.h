#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Arranca el escritorio. Bloquea hasta que el usuario sale (ESC).
void desktop_run(void);

#ifdef __cplusplus
}
#endif
