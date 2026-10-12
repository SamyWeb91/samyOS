// paging.h
#pragma once

/* =====================================================================
 * Flags de PTE (Page Table Entry)
 * ===================================================================== */
#define PTE_PRESENT  0x001   /* Page present */
#define PTE_RW       0x002   /* Read/Write */
#define PTE_USER     0x004   /* Accesible desde ring 3 */
#define PTE_PWT      0x008   /* Page Write-Through */
#define PTE_PCD      0x010   /* Page Cache Disable */
#define PTE_PAT      0x080   /* Page Attribute Table (bit 7 en PTE) */
#define PTE_WC       (PTE_PWT | PTE_PCD)   /* Write-Combining (sin PAT) */

#ifdef __cplusplus
extern "C" {
#endif

void paging_init();
void paging_enable_now();

void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
void paging_map_range(unsigned int virt, unsigned int phys, unsigned int size_kb, unsigned int flags);

unsigned int paging_get_pd_addr();
unsigned int paging_get_pt0_addr();

#ifdef __cplusplus
}
#endif
