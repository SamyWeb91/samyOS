#pragma once

void paging_init();
void paging_map(unsigned int virt, unsigned int phys, unsigned int flags);
void paging_unmap(unsigned int virt);
