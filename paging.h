#pragma once

// Llena el Page Directory y la Page Table. NO activa la paginacion.
void paging_init();

// Activa la paginacion cargando CR3 y poniendo el bit 31 de CR0.
// Llamar SOLO despues de paging_init y con cuidado.
void paging_enable_now();

// Devuelve la direccion fisica del Page Directory
unsigned int paging_get_pd_addr();

// Devuelve la direccion fisica de la Page Table 0
unsigned int paging_get_pt0_addr();
