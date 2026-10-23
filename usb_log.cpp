// usb_log.cpp
#include "usb.h"
#include "screen.h"

#define USB_LOG_MAX  128
#define USB_LOG_LINE 80

static char usb_log[USB_LOG_MAX][USB_LOG_LINE];
static int  usb_log_n = 0;

extern "C" void usb_log_add(const char *s) {
    if (usb_log_n >= USB_LOG_MAX) return;
    int i = 0;
    while (s[i] && i < USB_LOG_LINE - 1) { usb_log[usb_log_n][i] = s[i]; i++; }
    usb_log[usb_log_n][i] = 0;
    usb_log_n++;
}

extern "C" void usb_log_u32(const char *prefix, uint32_t v) {
    char buf[USB_LOG_LINE];
    int n = 0;
    while (prefix[n] && n < USB_LOG_LINE - 16) { buf[n] = prefix[n]; n++; }
    char tmp[12]; int t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v) { tmp[t++] = '0' + (v % 10); v /= 10; }
    for (int i = t - 1; i >= 0 && n < USB_LOG_LINE - 1; i--) buf[n++] = tmp[i];
    buf[n] = 0;
    usb_log_add(buf);
}

extern "C" int usb_log_count_get(void) { return usb_log_n; }
extern "C" const char *usb_log_get(int n) {
    if (n < 0 || n >= usb_log_n) return 0;
    return usb_log[n];
}
