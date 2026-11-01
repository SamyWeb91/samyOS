/*
 * Copyright (C) 2026 Samuel Tamayo Infante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
