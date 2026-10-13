// usb.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * API publica (la que usa el kernel)
 * ===================================================================== */

int      usb_init(void);
int      usb_is_ready(void);
uint32_t usb_get_sector_count(void);
int      usb_read_sector(uint32_t lba, void *buffer);

int         usb_log_count_get(void);
const char *usb_log_get(int n);

/* =====================================================================
 * API interna (la que usan los .cpp modulares entre si)
 * ===================================================================== */

/* --- log --- */
void usb_log_add(const char *s);
void usb_log_u32(const char *prefix, uint32_t v);

/*funcion agregada para usb_dev */
void *xhci_get_dcbaa(void);

/* --- xhci --- */
int  xhci_init(void);
int  xhci_send_cmd(uint32_t p1, uint32_t p2, uint32_t st, uint32_t ctrl,
                   uint32_t *out_type, uint32_t *out_status, uint64_t *out_param);
int  xhci_wait_event(uint32_t *type, uint64_t *param, uint32_t *status, int timeout);
void xhci_ring_doorbell(int slot, int target);

/* --- helpers xhci --- */
void *xhci_get_op_regs(void);
int   xhci_get_max_ports(void);

/* --- enumeracion --- */
int  usb_enumerate_devices(void);
int  usb_addr_device(int slot, int port);
int  usb_get_descriptor(int slot, int type, int index, void *buf, int len);
int  usb_set_config(int slot, int cfg);

/* --- msc --- */
int  usb_msc_init(int slot);
int  usb_msc_read_sector(int slot, uint32_t lba, void *buf);
int  usb_msc_get_sector_count(int slot);

#ifdef __cplusplus
}
#endif
