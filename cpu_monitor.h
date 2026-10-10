// cpu_monitor.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa el monitor. Llamar una vez al arrancar el desktop. */
void cpu_monitor_init(void);

/* Marca el inicio de una iteración del bucle principal. */
void cpu_monitor_begin_iter(void);

/* Marca que la CPU va a dormir (justo antes de hlt). */
void cpu_monitor_sleep_start(void);

/* Marca que la CPU ha salido de hlt (justo después). */
void cpu_monitor_sleep_end(void);

/* Cierra la iteración y actualiza el porcentaje si toca. */
void cpu_monitor_end_iter(void);

/* Devuelve el último porcentaje calculado (0..100). */
int  cpu_monitor_usage(void);

#ifdef __cplusplus
}
#endif
