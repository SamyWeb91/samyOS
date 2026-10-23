format ELF
section '.data' align 4

public desktop_bin
public desktop_bin_len

desktop_bin:
    file 'desktop.bin'
desktop_bin_end:

desktop_bin_len:
    dd desktop_bin_end - desktop_bin
