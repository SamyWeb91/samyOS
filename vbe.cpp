#include "vbe.h"
#include "screen.h"

// Info guardada por el bootloader en 0x6000:
//   0x6000: magic "VBE1" o 0
//   0x6004: framebuffer physical addr (u32)
//   0x6008: pitch (u16)
//   0x600A: width (u16)
//   0x600C: height (u16)
//   0x600E: bpp (u8)

struct VBEInfo {
    uint32_t magic;
    uint32_t fb_addr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
} __attribute__((packed));

static VBEInfo vbe;

extern "C" void vbe_init(void) {
    volatile uint32_t *p = (volatile uint32_t *)0x6000;

    vbe.magic  = p[0];
    vbe.fb_addr = p[1];
    vbe.pitch  = *(volatile uint16_t *)(0x6008);
    vbe.width  = *(volatile uint16_t *)(0x600A);
    vbe.height = *(volatile uint16_t *)(0x600C);
    vbe.bpp    = *(volatile uint8_t  *)(0x600E);
}

extern "C" int vbe_available(void) {
    return vbe.magic == 0x31454256;   // "VBE1"
}

extern "C" uint32_t vbe_fb_addr(void) { return vbe.fb_addr; }
extern "C" uint16_t vbe_pitch(void)   { return vbe.pitch; }
extern "C" uint16_t vbe_width(void)   { return vbe.width; }
extern "C" uint16_t vbe_height(void)  { return vbe.height; }
extern "C" uint8_t  vbe_bpp(void)     { return vbe.bpp; }
