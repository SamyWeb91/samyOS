// ahci.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * API principal del driver AHCI (solo lectura)
 * ===================================================================== */

/* Inicializa el driver. Devuelve 1 si ok, 0 si no hay AHCI o fallo. */
int      ahci_init(void);

/* Lee un sector (512 bytes) a `buffer`. Devuelve 1 si ok, 0 si error. */
int      ahci_read_sector(uint64_t lba, void *buffer);

/* Numero total de sectores del disco detectado. */
uint64_t ahci_get_sector_count(void);

/* 1 si el driver esta listo, 0 si no. */
int      ahci_is_ready(void);

/* Indice del puerto SATA donde se encontro el disco (0..31), o -1. */
int      ahci_get_port_index(void);

/* =====================================================================
 * Log interno (lo usa el comando `ahcilog` del shell)
 * ===================================================================== */

/* Numero de lineas en el log. */
int         ahci_log_count_get(void);

/* Devuelve la linea n-esima del log (0..count-1), o 0 si n fuera de rango. */
const char *ahci_log_get(int n);

/* =====================================================================
 * NOTA: no hay ahci_write_sector. Este driver es SOLO LECTURA.
 * ===================================================================== */

#ifdef __cplusplus
}
#endif
