#define _DEFAULT_SOURCE 1
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrender.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "microui_renderer.h"
#include "atlas.inl"

/* X11 & XRender state */
static Display  *dpy;
static Window    win;
static Visual   *s_visual;
static int       s_depth;
static int       win_width;
static int       win_height;

/* XRender picture for the window (direct rendering) */
static Picture   win_picture;

/* Font atlas picture (A8) */
static Picture   atlas_picture;

/* Input state */
static int r_keys[256];
static int r_mod;
static int r_mx;
static int r_my;
static int r_mouse_btn;
static int r_close_requested;
static Atom wm_delete_window;

/* Current clip rectangle */
static mu_Rect clip_rect;

/* Keycode mapping */
static int KEYCODES[124] = {
  XK_BackSpace,8,  XK_Delete,127,    XK_Down,18,       XK_End,5,
  XK_Escape,27,    XK_Home,2,        XK_Insert,26,     XK_Left,20,
  XK_Page_Down,4,  XK_Page_Up,3,     XK_Return,10,     XK_Right,19,
  XK_Tab,9,        XK_Up,17,         XK_apostrophe,39, XK_backslash,92,
  XK_bracketleft,91, XK_bracketright,93, XK_comma,44,  XK_equal,61,
  XK_grave,96,     XK_minus,45,      XK_period,46,     XK_semicolon,59,
  XK_slash,47,     XK_space,32,
  XK_a,65, XK_b,66, XK_c,67, XK_d,68, XK_e,69, XK_f,70,
  XK_g,71, XK_h,72, XK_i,73, XK_j,74, XK_k,75, XK_l,76,
  XK_m,77, XK_n,78, XK_o,79, XK_p,80, XK_q,81, XK_r,82,
  XK_s,83, XK_t,84, XK_u,85, XK_v,86, XK_w,87, XK_x,88,
  XK_y,89, XK_z,90,
  XK_0,48, XK_1,49, XK_2,50, XK_3,51, XK_4,52,
  XK_5,53, XK_6,54, XK_7,55, XK_8,56, XK_9,57,
};

static void process_events(void);

/* Convert mu_Color to XRenderColor (0‑65535 scale) */
static XRenderColor to_xrcolor(mu_Color c) {
    XRenderColor xc;
    xc.red   = (c.r << 8) | c.r;
    xc.green = (c.g << 8) | c.g;
    xc.blue  = (c.b << 8) | c.b;
    xc.alpha = (c.a << 8) | c.a;
    return xc;
}

/* Create a 1×1 solid picture with repeat enabled */
static Picture create_solid_picture(mu_Color color) {
    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, s_visual);
    if (!fmt) return None;

    Pixmap pix = XCreatePixmap(dpy, win, 1, 1, s_depth);
    if (!pix) return None;

    Picture pic = XRenderCreatePicture(dpy, pix, fmt, 0, NULL);
    XFreePixmap(dpy, pix);  /* picture holds a reference */
    if (!pic) return None;

    XRenderColor xc = to_xrcolor(color);
    XRenderFillRectangle(dpy, PictOpSrc, pic, &xc, 0, 0, 1, 1);

    XRenderPictureAttributes pa;
    pa.repeat = RepeatNormal;
    XRenderChangePicture(dpy, pic, CPRepeat, &pa);

    return pic;
}

/* Create the atlas picture (A8) from the in‑memory grayscale data */
static int create_atlas_picture(void) {
    XImage *img;
    Pixmap pix;
    GC gc;
    XRenderPictFormat *fmt;

    pix = XCreatePixmap(dpy, win, ATLAS_WIDTH, ATLAS_HEIGHT, 8);
    if (!pix) return -1;

    gc = XCreateGC(dpy, pix, 0, NULL);
    img = XCreateImage(dpy, NULL, 8, ZPixmap, 0, NULL,
                       ATLAS_WIDTH, ATLAS_HEIGHT, 8, 0);
    img->data = (char*)atlas_texture;
    XPutImage(dpy, pix, gc, img, 0, 0, 0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
    img->data = NULL;
    XDestroyImage(img);
    XFreeGC(dpy, gc);

    fmt = XRenderFindStandardFormat(dpy, PictStandardA8);
    if (!fmt) {
        XFreePixmap(dpy, pix);
        return -1;
    }

    atlas_picture = XRenderCreatePicture(dpy, pix, fmt, 0, NULL);
    XFreePixmap(dpy, pix);

    return atlas_picture ? 0 : -1;
}

/* Public functions */

int r_init(Display *display, Window window, GC context, Visual *visual, int depth, int width, int height) {
    dpy = display;
    win = window;
    s_visual = visual;
    s_depth = depth;
    win_width = width;
    win_height = height;

    /* Prevent X from painting the window background */
    XSetWindowBackgroundPixmap(dpy, win, None);

    int event_base, error_base;
    if (!XRenderQueryExtension(dpy, &event_base, &error_base)) {
        return -1;
    }

    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, visual);
    if (!fmt) return -1;

    win_picture = XRenderCreatePicture(dpy, win, fmt, 0, NULL);
    if (!win_picture) return -1;

    if (create_atlas_picture() < 0) return -1;

    /* Input selection */
    XSelectInput(dpy, win,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                 StructureNotifyMask);

    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);

    /* Initial clip rect */
    clip_rect = mu_rect(0, 0, width, height);
    XRenderSetPictureClipRectangles(dpy, win_picture, 0, 0,
                                    (XRectangle*)&clip_rect, 1);

    r_clear(mu_color(0, 0, 0, 255));
    return 0;
}

void r_update_context(Display *display, Window window, GC context) {
    dpy = display;
    win = window;
}

void r_resize(int width, int height) {
    win_width = width;
    win_height = height;
    clip_rect = mu_rect(0, 0, width, height);
    XRenderSetPictureClipRectangles(dpy, win_picture, 0, 0,
                                    (XRectangle*)&clip_rect, 1);
}

void r_clear(mu_Color clr) {
    XRenderColor xc = to_xrcolor(clr);
    XRenderFillRectangle(dpy, PictOpSrc, win_picture, &xc,
                         0, 0, win_width, win_height);
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
    XRenderColor xc = to_xrcolor(color);
    XRenderFillRectangle(dpy, PictOpOver, win_picture, &xc,
                         rect.x, rect.y, rect.w, rect.h);
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    Picture solid = create_solid_picture(color);
    if (solid == None) return;

    XRenderComposite(dpy, PictOpOver,
                     solid,               /* source (solid color) */
                     atlas_picture,       /* mask (alpha from atlas) */
                     win_picture,
                     0, 0,                 /* source x, y */
                     src.x, src.y,         /* mask x, y */
                     rect.x, rect.y,       /* dest x, y */
                     src.w, src.h);
    XRenderFreePicture(dpy, solid);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    Picture solid = create_solid_picture(color);
    if (solid == None) return;

    int x = pos.x;
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        XRenderComposite(dpy, PictOpOver,
                         solid, atlas_picture, win_picture,
                         0, 0, src.x, src.y,
                         x, pos.y, src.w, src.h);
        x += src.w;
    }
    XRenderFreePicture(dpy, solid);
}

int r_get_text_width(const char *text, int len) {
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

int r_get_text_height(void) {
    return 18;
}

void r_set_clip_rect(mu_Rect rect) {
    clip_rect = rect;
    XRenderSetPictureClipRectangles(dpy, win_picture, 0, 0,
                                    (XRectangle*)&clip_rect, 1);
}

void r_present(void) {
    XFlush(dpy);
    process_events();
}

void r_present_noevents(void) {
    XFlush(dpy);
}

/* Input handling - now properly updates all state */
static void process_events(void) {
    XEvent ev;
    while (XPending(dpy)) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case ButtonPress:
            r_mouse_btn = 1;
            /* Also update mouse position on button press */
            r_mx = ev.xbutton.x;
            r_my = ev.xbutton.y;
            break;
        case ButtonRelease:
            r_mouse_btn = 0;
            r_mx = ev.xbutton.x;
            r_my = ev.xbutton.y;
            break;
        case MotionNotify:
            r_mx = ev.xmotion.x;
            r_my = ev.xmotion.y;
            break;
        case KeyPress:
        case KeyRelease: {
            int m = ev.xkey.state;
            int k = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
            int is_press = (ev.type == KeyPress);

            /* Update key states */
            for (unsigned int i = 0; i < 124; i += 2) {
                if (KEYCODES[i] == k) {
                    r_keys[KEYCODES[i + 1]] = is_press;
                    break;
                }
            }

            /* Update modifier states */
            r_mod = ((m & ControlMask) ? 1 : 0) |
                    ((m & ShiftMask)   ? 2 : 0) |
                    ((m & Mod1Mask)    ? 4 : 0) |
                    ((m & Mod4Mask)    ? 8 : 0);
            break;
        }
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete_window) {
                r_close_requested = 1;
            }
            break;
        case ConfigureNotify:
            if (ev.xconfigure.width  != win_width ||
                ev.xconfigure.height != win_height) {
                r_resize(ev.xconfigure.width, ev.xconfigure.height);
            }
            break;
        }
    }
}

static int mouse_down = 0;

int r_mouse_down(void) {
    if (r_mouse_btn && !mouse_down) {
        mouse_down = 1;
        return 1;
    }
    return 0;
}

int r_mouse_up(void) {
    if (!r_mouse_btn && mouse_down) {
        mouse_down = 0;
        return 1;
    }
    return 0;
}

int r_mouse_moved(int *mousex, int *mousey) {
    if (r_mx != *mousex || r_my != *mousey) {
        *mousex = r_mx;
        *mousey = r_my;
        return 1;
    }
    return 0;
}

int r_should_close(void)  { return r_close_requested; }

int r_ctrl_pressed(void)  { return (r_mod & 1) ? 1 : 0; }
int r_shift_pressed(void) { return (r_mod & 2) ? 1 : 0; }
int r_alt_pressed(void)   { return (r_mod & 4) ? 1 : 0; }

int r_key_down(int key) {
    if (r_keys[key] == 1) {
        r_keys[key]++;
        return 1;
    }
    return 0;
}

int r_key_up(int key) {
    if (r_keys[key] == 0) {
        return 1;
    }
    return 0;
}

int64_t r_get_time(void) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * 1000 + (t.tv_nsec / 1000000);
}

void r_sleep(int64_t ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}