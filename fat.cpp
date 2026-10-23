// fat.cpp
#include "fat.h"
#include "fat32.h"
#include "shell.h"
#include "win.h"

extern "C" void shell_term_print(int wid, const char *s);
extern "C" void shell_term_println(int wid, const char *s);
extern "C" void shell_print_uint(int wid, unsigned int v);

#define tp(w,s)  shell_term_print((w),(s))
#define tpl(w,s) shell_term_println((w),(s))
#define tpu(w,v) shell_print_uint((w),(v))

/* Buffer para leer archivos (maximo 32 KB por ahora) */
#define FAT_READ_MAX (32 * 1024)
static uint8_t g_read_buf[FAT_READ_MAX];

/* =====================================================================
 * Helpers
 * ===================================================================== */
static unsigned int parse_uint(const char *s) {
    unsigned int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned int)(*s - '0');
        s++;
    }
    return v;
}

static void print_dec(int wid, unsigned int v) {
    char buf[16];
    int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = '0' + (v % 10); v /= 10; }
    char out[16];
    int k = 0;
    for (int i = n - 1; i >= 0; i--) out[k++] = buf[i];
    out[k] = 0;
    tp(wid, out);
}

/* =====================================================================
 * Comando: fatmount <lba>
 * ===================================================================== */
int fat_cmd_mount(int wid, const char *arg)
{
    if (!arg || !*arg) {
        tpl(wid, "uso: fatmount <lba_inicio_particion>");
        tpl(wid, "  (mira 'partitions' para ver el LBA de inicio)");
        return 1;
    }

    unsigned int lba = parse_uint(arg);

    if (fat32_is_mounted()) {
        fat32_unmount();
    }

    if (!fat32_mount(lba)) {
        tpl(wid, "fatmount: error al montar");
        tpl(wid, "  comprueba que el LBA es correcto y la particion es FAT32");
        return 1;
    }

    const Fat32Volume *v = fat32_get_volume();
    tp(wid, "fatmount: OK en LBA ");
    print_dec(wid, lba);
    tpl(wid, "");

    tp(wid, "  Etiqueta: ");
    tpl(wid, v->volume_label);

    tp(wid, "  FS: ");
    tpl(wid, v->fs_type);

    tp(wid, "  Cluster: ");
    print_dec(wid, v->bytes_per_cluster);
    tpl(wid, " bytes");

    tp(wid, "  Root cluster: ");
    print_dec(wid, v->root_cluster);
    tpl(wid, "");

    return 1;
}

/* =====================================================================
 * Comando: fatinfo
 * ===================================================================== */
int fat_cmd_info(int wid)
{
    if (!fat32_is_mounted()) {
        tpl(wid, "fatinfo: no hay volumen montado (usa fatmount <lba>)");
        return 1;
    }

    const Fat32Volume *v = fat32_get_volume();

    tpl(wid, "=== FAT32 INFO ===");
    tp(wid, "Particion en LBA: "); print_dec(wid, v->part_lba); tpl(wid, "");
    tp(wid, "Etiqueta: "); tpl(wid, v->volume_label);
    tp(wid, "FS: "); tpl(wid, v->fs_type);
    tp(wid, "Bytes/sector: "); print_dec(wid, v->bytes_per_sector); tpl(wid, "");
    tp(wid, "Sectores/cluster: "); print_dec(wid, v->sectors_per_cluster); tpl(wid, "");
    tp(wid, "Bytes/cluster: "); print_dec(wid, v->bytes_per_cluster); tpl(wid, "");
    tp(wid, "Sectores reservados: "); print_dec(wid, v->reserved_sectors); tpl(wid, "");
    tp(wid, "Num FATs: "); print_dec(wid, v->num_fats); tpl(wid, "");
    tp(wid, "Tamano FAT (sectores): "); print_dec(wid, v->fat_size_sectors); tpl(wid, "");
    tp(wid, "Root cluster: "); print_dec(wid, v->root_cluster); tpl(wid, "");
    tp(wid, "Total clusters: "); print_dec(wid, v->total_clusters); tpl(wid, "");
    tp(wid, "Capacidad ~"); 
    print_dec(wid, (v->total_clusters * v->bytes_per_cluster) / (1024 * 1024));
    tpl(wid, " MiB");

    return 1;
}

/* =====================================================================
 * Callback para listar
 * ===================================================================== */
static int g_list_count;

static void list_cb(const char *name, uint32_t size, uint8_t attr, uint32_t fc)
{
    (void)fc;
    int wid = win_get_focused();

    tp(wid, "  ");
    tp(wid, name);

    /* Rellenar hasta 40 chars */
    uint32_t l = 0;
    while (name[l]) l++;
    for (uint32_t i = l; i < 40; i++) tp(wid, " ");

    if (attr & FAT32_ATTR_DIRECTORY) {
        tpl(wid, "<DIR>");
    } else {
        print_dec(wid, size);
        tpl(wid, " bytes");
    }

    g_list_count++;
}

/* =====================================================================
 * Comando: fatls [path]
 * ===================================================================== */
int fat_cmd_ls(int wid, const char *arg)
{
    if (!fat32_is_mounted()) {
        tpl(wid, "fatls: no hay volumen montado");
        return 1;
    }

    const char *path = (arg && *arg) ? arg : "/";

    g_list_count = 0;
    tp(wid, "Listado de ");
    tpl(wid, path);

    if (!fat32_list(path, list_cb)) {
        tpl(wid, "fatls: error al listar");
        return 1;
    }

    tp(wid, "  (");
    print_dec(wid, g_list_count);
    tpl(wid, " entradas)");

    return 1;
}

/* =====================================================================
 * Comando: fatcat <path>
 * ===================================================================== */
int fat_cmd_cat(int wid, const char *arg)
{
    if (!fat32_is_mounted()) {
        tpl(wid, "fatcat: no hay volumen montado");
        return 1;
    }
    if (!arg || !*arg) {
        tpl(wid, "uso: fatcat <path>");
        return 1;
    }

    int n = fat32_read_file(arg, g_read_buf, FAT_READ_MAX - 1);
    if (n < 0) {
        tpl(wid, "fatcat: no se pudo leer");
        return 1;
    }

    g_read_buf[n] = 0;

    tp(wid, "--- ");
    tp(wid, arg);
    tpl(wid, " ---");

    /* Imprimir respetando los \n */
    char line[128];
    int k = 0;
    for (int i = 0; i < n; i++) {
        char c = (char)g_read_buf[i];
        if (c == '\n' || c == '\r') {
            line[k] = 0;
            tpl(wid, line);
            k = 0;
        } else if (k < 127) {
            line[k++] = c;
        }
    }
    if (k > 0) {
        line[k] = 0;
        tpl(wid, line);
    }

    tp(wid, "--- ");
    print_dec(wid, n);
    tpl(wid, " bytes ---");

    return 1;
}

/* =====================================================================
 * Comando: fatstat <path>
 * ===================================================================== */
int fat_cmd_stat(int wid, const char *arg)
{
    if (!fat32_is_mounted()) {
        tpl(wid, "fatstat: no hay volumen montado");
        return 1;
    }
    if (!arg || !*arg) {
        tpl(wid, "uso: fatstat <path>");
        return 1;
    }

    if (!fat32_exists(arg)) {
        tpl(wid, "fatstat: no existe");
        return 1;
    }

    tp(wid, "Path: ");
    tpl(wid, arg);

    if (fat32_is_dir(arg)) {
        tpl(wid, "Tipo: directorio");
    } else {
        tpl(wid, "Tipo: archivo");
        tp(wid, "Tamano: ");
        print_dec(wid, (unsigned int)fat32_file_size(arg));
        tpl(wid, " bytes");
    }

    return 1;
}

/* =====================================================================
 * Init
 * ===================================================================== */
void fat_init(void) {
    fat32_unmount();
}
