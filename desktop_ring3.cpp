// desktop_ring3.cpp
#include "desktop.h"
#include "gfx.h"
#include "composer.h"
#include "taskbar_start.h"
#include "shell.h"
#include "win.h"
#include "syscall.h"

extern "C" int desktop_main(void);

static inline unsigned int u_ticks(void) { return (unsigned int)sys_ticks(); }
static inline int u_key(void)            { return (int)sys_key_poll(); }
static inline void u_yield(void)         { sys_yield(); }

static inline int u_mouse(struct mouse_event_user *e) {
    return (int)sys_mouse_poll(e);
}

static int next_term_x = 60;
static int next_term_y = 60;
static int next_term_n = 1;

static void start_action(int action) {
    if (action == 1) {
        char title[16];
        title[0]='T'; title[1]='e'; title[2]='r'; title[3]='m';
        title[4]=' '; title[5]='0' + (next_term_n % 10); title[6]=0;
        shell_new_terminal(next_term_x, next_term_y, 420, 280, title);
        next_term_x += 30;
        next_term_y += 30;
        if (next_term_x > 400) { next_term_x = 60; next_term_y = 60; }
        next_term_n++;
        composer_force_redraw();
    }
}

extern "C" int desktop_main(void) {
    if (!gfx_ready()) return 1;

    composer_init();
    shell_init();
    taskbar_start_init(start_action);

    int wid = shell_new_terminal(60, 60, 420, 280, "Terminal 1");
    composer_set_terminal(wid);
    composer_force_redraw();

    while (1) {
        int did_work = 0;
        unsigned int now = u_ticks();

        if (composer_periodic(now)) did_work = 1;

        struct mouse_event_user me;
        if (u_mouse(&me) == 0) {
            if (me.dx || me.dy || me.buttons) {
                extern int composer_mouse_event(int x, int y,
                                                int dx, int dy, int buttons);
                if (composer_mouse_event(me.x, me.y, me.dx, me.dy, me.buttons))
                    did_work = 1;
            }
        }

        int k = u_key();
        if (k >= 0) {
            int fid = win_get_focused();
            if (fid >= 0 && shell_is_terminal(fid)) {
                shell_handle_key(fid, (char)k);
                composer_dirty_window(fid);
            }
            did_work = 1;
        }

        if (composer_has_dirty()) {
            composer_apply();
            did_work = 1;
        }

        u_yield();
    }
    return 0;
}

extern "C" int main(void) { return desktop_main(); }
