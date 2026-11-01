// Copyright (C) 2026 Samuel Tamayo Infante
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// folder.cpp
#include "folder.h"
#include "mfs.h"
#include "win.h"
#include "gfx.h"
#include "composer.h"

/* =====================================================================
 * Estado
 * ===================================================================== */
#define FOLDER_MAX 4
#define ROWS_MAX   12
#define ROW_H      20
#define HEADER_H   40
#define PADDING    6
#define BTN_W      20
#define BTN_H      16

typedef struct {
    int  wid;             /* -1 si libre */
    int  selected;        /* indice del archivo seleccionado */
    int  count;           /* numero de archivos */
} FolderTerm;

static FolderTerm folders[FOLDER_MAX];
static int folder_ready = 0;

/* =====================================================================
 * Helpers
 * ===================================================================== */
static int str_len(const char *s) {
    int n = 0; while (s[n]) n++; return n;
}

static void print_at(int wid, const char *s) {
    if (wid < 0) return;
    win_sys_write(wid, s, str_len(s));
}

static void print_uint_at(int wid, unsigned int v) {
    char tmp[16]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    char out[16]; int k = 0;
    for (int i = n - 1; i >= 0; i--) out[k++] = tmp[i];
    out[k] = 0;
    print_at(wid, out);
}

/* =====================================================================
 * Dibujo
 * ===================================================================== */
static void folder_draw(int wid, FolderTerm *f) {
    win_sys_clear(wid);

    print_at(wid, "=== ARCHIVOS ===\n");
    print_at(wid, "Click=seleccionar  X=borrar\n");
    print_at(wid, "n=nuevo  j/k=navegar  ESC=cerrar\n");
    print_at(wid, "----------------------------\n");

    f->count = mfs_count();

    if (f->count == 0) {
        print_at(wid, "  (vacio)\n");
    } else {
        for (int i = 0; i < f->count && i < ROWS_MAX; i++) {
            char name[16];
            unsigned int size = 0;
            if (mfs_get(i, name, &size) != 0) continue;

            if (i == f->selected) print_at(wid, "> ");
            else                  print_at(wid, "  ");

            print_at(wid, name);

            for (int k = str_len(name); k < 14; k++) print_at(wid, " ");

            print_uint_at(wid, size);
            print_at(wid, " bytes  [X]\n");
        }
    }

    print_at(wid, "----------------------------\n");
    print_at(wid, "Total: ");
    print_uint_at(wid, (unsigned int)f->count);
    print_at(wid, " archivos\n");

    composer_dirty_window(wid);
}

/* =====================================================================
 * API publica
 * ===================================================================== */
extern "C" int folder_open(int x, int y, int w, int h) {
    if (!folder_ready) {
        for (int i = 0; i < FOLDER_MAX; i++) {
            folders[i].wid = -1;
            folders[i].selected = -1;
            folders[i].count = 0;
        }
        folder_ready = 1;
    }

    int slot = -1;
    for (int i = 0; i < FOLDER_MAX; i++) {
        if (folders[i].wid < 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int wid = win_sys_create(x, y, w, h, "Archivos");
    if (wid < 0) return -1;

    win_focus(wid);
    folders[slot].wid = wid;
    folders[slot].selected = -1;
    folders[slot].count = 0;

    folder_draw(wid, &folders[slot]);
    composer_dirty_window(wid);
    return wid;
}

extern "C" void folder_handle_key(int wid, char c) {
    FolderTerm *f = 0;
    for (int i = 0; i < FOLDER_MAX; i++) {
        if (folders[i].wid == wid) { f = &folders[i]; break; }
    }
    if (!f) return;

    if (c == 27) {   /* ESC: cerrar */
        composer_dirty_window(wid);
        f->wid = -1;
        win_sys_destroy(wid);
        return;
    }

    if (c == 'n' || c == 'N') {
        /* Crear archivo con nombre automatico */
        char name[16];
        name[0] = 'n'; name[1] = 'u'; name[2] = 'e'; name[3] = 'v';
        name[4] = 'o'; name[5] = '0'; name[6] = '.'; name[7] = 't';
        name[8] = 'x'; name[9] = 't'; name[10] = 0;
        for (int n = 0; n < 10; n++) {
            name[5] = '0' + n;
            if (mfs_create(name) == 0) break;
        }
        folder_draw(wid, f);
        return;
    }

    if (c == 'j' || c == 'J') {
        if (f->count > 0) {
            f->selected++;
            if (f->selected >= f->count) f->selected = f->count - 1;
            folder_draw(wid, f);
        }
        return;
    }

    if (c == 'k' || c == 'K') {
        if (f->count > 0) {
            f->selected--;
            if (f->selected < 0) f->selected = 0;
            folder_draw(wid, f);
        }
        return;
    }

    if (c == 'd' || c == 'D') {
        if (f->selected >= 0 && f->selected < f->count) {
            char name[16];
            unsigned int size;
            if (mfs_get(f->selected, name, &size) == 0) {
                mfs_delete(name);
                if (f->selected >= f->count - 1) f->selected = f->count - 2;
                folder_draw(wid, f);
            }
        }
        return;
    }
}

extern "C" void folder_handle_click(int wid, int mx, int my) {
    FolderTerm *f = 0;
    for (int i = 0; i < FOLDER_MAX; i++) {
        if (folders[i].wid == wid) { f = &folders[i]; break; }
    }
    if (!f) return;

    int wx, wy, ww, wh;
    if (win_get_rect(wid, &wx, &wy, &ww, &wh) != 0) return;

    /* Coordenadas locales dentro de la ventana */
    int lx = mx - wx;
    int ly = my - wy;

    /* Cada fila empieza despues de 4 lineas de 16 px = 64 px */
    int list_y0 = 4 * 16;
    if (ly < list_y0) return;

    int row = (ly - list_y0) / 16;

    if (row >= 0 && row < f->count && row < ROWS_MAX) {
        /* Si el click esta en la zona del boton [X] (a la derecha) */
        if (lx > 90) {
            char name[16];
            unsigned int size;
            if (mfs_get(row, name, &size) == 0) {
                mfs_delete(name);
                if (f->selected >= f->count - 1) f->selected = f->count - 2;
                folder_draw(wid, f);
                return;
            }
        }
        /* Si no, seleccionar */
        f->selected = row;
        folder_draw(wid, f);
    }
}

extern "C" int folder_is_folder(int wid) {
    if (wid < 0) return 0;
    for (int i = 0; i < FOLDER_MAX; i++) {
        if (folders[i].wid == wid) return 1;
    }
    return 0;
}

extern "C" void folder_close(int wid) {
    for (int i = 0; i < FOLDER_MAX; i++) {
        if (folders[i].wid == wid) {
            folders[i].wid = -1;
            return;
        }
    }
}
