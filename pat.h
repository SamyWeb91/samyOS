// pat.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Configura PAT para que la entrada 1 sea Write-Combining.
   Idempotente: si ya está configurada, no hace nada.
   Devuelve 0 si ok, -1 si la CPU no soporta PAT. */
int pat_init(void);

/* Lee el TSC (Time Stamp Counter) con serialización. */
unsigned long long pat_rdtsc(void);

#ifdef __cplusplus
}
#endif
