// usb_msc.cpp
#include <stdint.h>
#include "usb.h"
#include "screen.h"

/* CBW (Command Block Wrapper) - 31 bytes */
struct cbw {
    uint32_t signature;      /* 0x43425355 "USBC" */
    uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t  flags;          /* bit 7: 0=OUT, 1=IN */
    uint8_t  lun;
    uint8_t  cb_length;      /* 1..16 */
    uint8_t  cb[16];
} __attribute__((packed));

/* CSW (Command Status Wrapper) - 13 bytes */
struct csw {
    uint32_t signature;      /* 0x53425355 "USBS" */
    uint32_t tag;
    uint32_t residue;
    uint8_t  status;
} __attribute__((packed));

static int msc_slot = -1;
static uint32_t msc_num_sectors = 0;
static uint32_t msc_tag = 1;

/* Buffers */
static uint8_t msc_buf[512] __attribute__((aligned(64)));
static struct cbw cbw_s;
static struct csw csw_s;

/* Endpoint Bulk OUT/IN: hay que leerlos del descriptor de configuracion.
   Simplificacion: asumimos que son ep 1 OUT y ep 2 IN (bulk), con
   max packet 64. En un driver real hay que parsear el descriptor. */
static uint8_t bulk_out_ep = 1;
static uint8_t bulk_in_ep  = 2;
static uint16_t bulk_max_packet = 64;

/* =====================================================================
 * Enviar comando BOT: CBW -> Data (si aplica) -> CSW
 * ===================================================================== */
static int msc_transfer(int dir_in, uint32_t data_len, uint8_t *data) {
    (void)dir_in; (void)data_len; (void)data;
    /* Pendiente de implementar el envio real.
       Requiere:
       1. Construir el CBW en msc_buf.
       2. Enviar por el endpoint Bulk OUT un TRB Normal.
       3. Esperar Transfer Event.
       4. Si dir_in, enviar TRB por Bulk IN.
       5. Esperar CSW.
       Eso requiere configurar los endpoints Bulk del slot (add flags en
       Input Context, Configure Endpoint).
       ~200 lineas mas de codigo. */
    return -1;
}

/* =====================================================================
 * Enviar SCSI INQUIRY
 * ===================================================================== */
static int msc_inquiry(void) {
    struct cbw *cbw = &cbw_s;
    cbw->signature = 0x43425355;
    cbw->tag = msc_tag++;
    cbw->data_transfer_length = 36;
    cbw->flags = 0x80;   /* IN */
    cbw->lun = 0;
    cbw->cb_length = 6;
    cbw->cb[0] = 0x12;   /* INQUIRY */
    cbw->cb[1] = 0;
    cbw->cb[2] = 0;
    cbw->cb[3] = 0;
    cbw->cb[4] = 36;
    cbw->cb[5] = 0;

    return msc_transfer(1, 36, msc_buf);
}

/* =====================================================================
 * Enviar SCSI READ CAPACITY(10)
 * ===================================================================== */
static int msc_read_capacity(void) {
    struct cbw *cbw = &cbw_s;
    cbw->signature = 0x43425355;
    cbw->tag = msc_tag++;
    cbw->data_transfer_length = 8;
    cbw->flags = 0x80;
    cbw->lun = 0;
    cbw->cb_length = 10;
    cbw->cb[0] = 0x25;   /* READ CAPACITY(10) */
    cbw->cb[1] = 0;
    cbw->cb[2] = 0; cbw->cb[3] = 0; cbw->cb[4] = 0; cbw->cb[5] = 0;
    cbw->cb[6] = 0; cbw->cb[7] = 0; cbw->cb[8] = 0; cbw->cb[9] = 0;

    if (msc_transfer(1, 8, msc_buf) != 0) return -1;

    /* Respuesta: 4 bytes LBA + 4 bytes block length */
    uint32_t last_lba = msc_buf[0] | (msc_buf[1] << 8) | (msc_buf[2] << 16) | (msc_buf[3] << 24);
    msc_num_sectors = last_lba + 1;
    return 0;
}

/* =====================================================================
 * Enviar SCSI READ(10) para 1 sector
 * ===================================================================== */
static int msc_read10(uint32_t lba, void *out) {
    struct cbw *cbw = &cbw_s;
    cbw->signature = 0x43425355;
    cbw->tag = msc_tag++;
    cbw->data_transfer_length = 512;
    cbw->flags = 0x80;
    cbw->lun = 0;
    cbw->cb_length = 10;
    cbw->cb[0] = 0x28;   /* READ(10) */
    cbw->cb[1] = 0;
    cbw->cb[2] = (lba >> 24) & 0xFF;
    cbw->cb[3] = (lba >> 16) & 0xFF;
    cbw->cb[4] = (lba >> 8) & 0xFF;
    cbw->cb[5] = lba & 0xFF;
    cbw->cb[6] = 0;
    cbw->cb[7] = 0;
    cbw->cb[8] = 0;
    cbw->cb[9] = 1;   /* num bloques = 1 */

    if (msc_transfer(1, 512, msc_buf) != 0) return -1;

    for (int i = 0; i < 512; i++) ((uint8_t *)out)[i] = msc_buf[i];
    return 0;
}

/* =====================================================================
 * Init de MSC
 * ===================================================================== */
extern "C" int usb_msc_init(int slot) {
    usb_log_add("msc: init");
    msc_slot = slot;

    /* Simplificacion: no configuramos endpoints Bulk. Devolvemos -1 hasta
       que se implemente. */
    usb_log_add("msc: pendiente configurar Bulk endpoints");
    return -1;
}

extern "C" int usb_msc_read_sector(int slot, uint32_t lba, void *buf) {
    if (slot != msc_slot) return -1;
    return msc_read10(lba, buf);
}

extern "C" int usb_msc_get_sector_count(int slot) {
    if (slot != msc_slot) return 0;
    return (int)msc_num_sectors;
}
