#define _DEFAULT_SOURCE 1#include <X11/XKBlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrender.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "microui_renderer.h"
#include "atlas.inl"   /* must define ATLAS_WIDTH, ATLAS_HEIGHT, ATLAS_WHITE, ATLAS_FONT, atlas[] and atlas_texture[] */

static Display * dpy;
static Window win;
static GC gc; /* kept for compatibility with your host code */
static Visual * s_visual;
static int s_depth;
static int win_width;
static int win_height;

/* XRender state */
static Picture window_picture;
static Picture atlas_picture;
static Pixmap atlas_pixmap;

/* Double-buffer state */
static Pixmap back_buffer_pixmap;
static Picture back_buffer_picture;

/* Input state */
static int r_keys[256];
static int r_mod;
static int r_mx;
static int r_my;
static int r_mouse_btn;
static int r_close_requested;
static Atom wm_delete_window;
static int needs_redraw;  /* Flag for expose events */

/* Last clear color - stored for expose redraws */
static mu_Color last_clear_color;

/* clang-format off */
static int KEYCODES[124] = {
  XK_BackSpace,8, XK_Delete,127, XK_Down,18, XK_End,5,
  XK_Escape,27, XK_Home,2, XK_Insert,26, XK_Left,20,
  XK_Page_Down,4, XK_Page_Up,3, XK_Return,10, XK_Right,19,
  XK_Tab,9, XK_Up,17, XK_apostrophe,39, XK_backslash,92,
  XK_bracketleft,91, XK_bracketright,93, XK_comma,44, XK_equal,61,
  XK_grave,96, XK_minus,45, XK_period,46, XK_semicolon,59,
  XK_slash,47, XK_space,32,
  XK_a,65, XK_b,66, XK_c,67, XK_d,68, XK_e,69, XK_f,70,
  XK_g,71, XK_h,72, XK_i,73, XK_j,74, XK_k,75, XK_l,76,
  XK_m,77, XK_n,78, XK_o,79, XK_p,80, XK_q,81, XK_r,82,
  XK_s,83, XK_t,84, XK_u,85, XK_v,86, XK_w,87, XK_x,88,
  XK_y,89, XK_z,90,
  XK_0,48, XK_1,49, XK_2,50, XK_3,51, XK_4,52,
  XK_5,53, XK_6,54, XK_7,55, XK_8,56, XK_9,57,
};
/* clang-format on */

static XRenderColor to_xrender_color(mu_Color c) {
  XRenderColor xc;
  xc.red = (uint16_t) c.r << 8;
  xc.green = (uint16_t) c.g << 8;
  xc.blue = (uint16_t) c.b << 8;
  xc.alpha = (uint16_t) c.a << 8;
  return xc;
}

int r_init(Display * display, Window window, GC context, Visual * visual, int depth, int width, int height) {
  dpy = display;
  win = window;
  gc = context;
  s_visual = visual;
  s_depth = depth;
  win_width = width;
  win_height = height;
  needs_redraw = 1;

  XSelectInput(dpy, win,
    ExposureMask | KeyPressMask | KeyReleaseMask |
    ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
    StructureNotifyMask);

  wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, & wm_delete_window, 1);

  /* XRender setup */
  int dummy;
  if (!XRenderQueryExtension(dpy, & dummy, & dummy)) {
    /* XRender is required for this renderer */
    return -1;
  }

  XRenderPictFormat * vis_fmt = XRenderFindVisualFormat(dpy, s_visual);
  if (!vis_fmt) {
    /* Fallback for visuals not registered in XRender's format table (common
       with 32-bit ARGB visuals used by compositors for native transparency). */
    vis_fmt = XRenderFindStandardFormat(dpy, s_depth == 32
                                            ? PictStandardARGB32
                                            : PictStandardRGB24);
  }
  if (!vis_fmt) return -1;

  window_picture = XRenderCreatePicture(dpy, win, vis_fmt, 0, NULL);

  back_buffer_pixmap = XCreatePixmap(dpy, win, width, height, s_depth);
  back_buffer_picture = XRenderCreatePicture(dpy, back_buffer_pixmap, vis_fmt, 0, NULL);

  XRenderPictFormat * a8_fmt = XRenderFindStandardFormat(dpy, PictStandardA8);
  atlas_pixmap = XCreatePixmap(dpy, win, ATLAS_WIDTH, ATLAS_HEIGHT, 8);

  GC atlas_gc = XCreateGC(dpy, atlas_pixmap, 0, NULL);
  XImage * atlas_img = XCreateImage(dpy, s_visual, 8, ZPixmap, 0,
    NULL, ATLAS_WIDTH, ATLAS_HEIGHT, 8, ATLAS_WIDTH);
  atlas_img -> data = (char * ) atlas_texture;
  XPutImage(dpy, atlas_pixmap, atlas_gc, atlas_img, 0, 0, 0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
  atlas_img -> data = NULL;
  XDestroyImage(atlas_img);
  XFreeGC(dpy, atlas_gc);

  atlas_picture = XRenderCreatePicture(dpy, atlas_pixmap, a8_fmt, 0, NULL);

  /* initial full clip + clear */
  r_set_clip_rect(mu_rect(0, 0, width, height));
  r_clear(mu_color(0, 0, 0, 255));

  return 0;
}

void r_update_context(Display * display, Window window, GC context) {
  dpy = display;
  win = window;
  gc = context;
}

void r_detach(void) {
  if (window_picture) {
    XRenderFreePicture(dpy, window_picture);
    window_picture = None;
  }
  win = None;
}

void r_switch_window(Window new_win, int width, int height) {
  if (window_picture) {
    XRenderFreePicture(dpy, window_picture);
    window_picture = None;
  }
  win = new_win;
  /* Ensure the new window receives the same event types r_init would set. */
  XSelectInput(dpy, win,
    ExposureMask | KeyPressMask | KeyReleaseMask |
    ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
    StructureNotifyMask);
  XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, s_visual);
  if (!fmt)
    fmt = XRenderFindStandardFormat(dpy, s_depth == 32
                                        ? PictStandardARGB32
                                        : PictStandardRGB24);
  if (fmt)
    window_picture = XRenderCreatePicture(dpy, win, fmt, 0, NULL);
  r_resize(width, height);
}

void r_resize(int width, int height) {
  if (!dpy || !back_buffer_picture) return;
  XRenderFreePicture(dpy, back_buffer_picture);
  XFreePixmap(dpy, back_buffer_pixmap);

  XRenderPictFormat * vis_fmt = XRenderFindVisualFormat(dpy, s_visual);
  if (!vis_fmt)
    vis_fmt = XRenderFindStandardFormat(dpy, s_depth == 32
                                            ? PictStandardARGB32
                                            : PictStandardRGB24);
  back_buffer_pixmap = XCreatePixmap(dpy, win, width, height, s_depth);
  back_buffer_picture = XRenderCreatePicture(dpy, back_buffer_pixmap, vis_fmt, 0, NULL);

  win_width = width;
  win_height = height;
  r_set_clip_rect(mu_rect(0, 0, width, height));
}

int r_needs_redraw(void) {
  int needs = needs_redraw;
  needs_redraw = 0;
  return needs;
}

/* ===================== DRAWING (XRender) ===================== */

void r_draw_rect(mu_Rect rect, mu_Color color) {
  if (rect.w <= 0 || rect.h <= 0) return;
  XRenderColor xc = to_xrender_color(color);
  XRenderFillRectangle(dpy, PictOpOver, back_buffer_picture, & xc,
    rect.x, rect.y, rect.w, rect.h);
}

void r_draw_text(const char * text, mu_Vec2 pos, mu_Color color) {
  if (!text || ! * text) return;

  XRenderColor xc = to_xrender_color(color);
  Picture solid = XRenderCreateSolidFill(dpy, & xc);

  mu_Rect dst = {
    pos.x,
    pos.y,
    0,
    0
  };
  for (const char * p = text;* p; p++) {
    if (( * p & 0xc0) == 0x80) continue;
    int chr = mu_min((unsigned char) * p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;

    XRenderComposite(dpy, PictOpOver,
      solid, atlas_picture, back_buffer_picture,
      0, 0, /* src (solid) coords ignored */
      src.x, src.y, /* mask coords in atlas */
      dst.x, dst.y, src.w, src.h);
    dst.x += dst.w;
  }
  XRenderFreePicture(dpy, solid);
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;

  XRenderColor xc = to_xrender_color(color);
  Picture solid = XRenderCreateSolidFill(dpy, & xc);

  XRenderComposite(dpy, PictOpOver,
    solid, atlas_picture, back_buffer_picture,
    0, 0, src.x, src.y,
    x, y, src.w, src.h);

  XRenderFreePicture(dpy, solid);
}

int r_get_text_width(const char * text, int len) {
  int res = 0;
  for (const char * p = text;* p && len--; p++) {
    if (( * p & 0xc0) == 0x80) continue;
    int chr = mu_min((unsigned char) * p, 127);
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}

int r_get_text_height(void) {
  return 18;
}

void r_set_clip_rect(mu_Rect r) {
  int x = mu_max(0, r.x);
  int y = mu_max(0, r.y);
  int w = mu_min(win_width, r.x + r.w) - x;
  int h = mu_min(win_height, r.y + r.h) - y;

  if (w <= 0 || h <= 0) {
    XRectangle zero = {
      0,
      0,
      0,
      0
    };
    XRenderSetPictureClipRectangles(dpy, back_buffer_picture, 0, 0, & zero, 1);
    return;
  }

  XRectangle clip = {
    (short) x,
    (short) y,
    (unsigned short) w,
    (unsigned short) h
  };
  XRenderSetPictureClipRectangles(dpy, back_buffer_picture, 0, 0, & clip, 1);
}

void r_clear(mu_Color clr) {
  last_clear_color = clr;  /* Store for expose events */

  /* clear always affects the whole window */
  XRectangle full = { 0, 0, (unsigned short) win_width, (unsigned short) win_height };
  XRenderSetPictureClipRectangles(dpy, back_buffer_picture, 0, 0, & full, 1);

  XRenderColor xc = to_xrender_color(clr);
  XRenderFillRectangle(dpy, PictOpSrc, back_buffer_picture, & xc, 0, 0, win_width, win_height);
}

void r_present(void) {
  /* Copy back buffer to window atomically */
  XRenderComposite(dpy, PictOpSrc, back_buffer_picture, None, window_picture,
    0, 0, 0, 0, 0, 0, win_width, win_height);
  XFlush(dpy);
}

int r_width() {
  return win_width;
}

int r_height() {
  return win_height;
}

void r_process_events(void) {
  /* event processing stays exactly as before */
  XEvent ev;
  while (XPending(dpy)) {
    XNextEvent(dpy, & ev);
    switch (ev.type) {
    case ButtonPress:
    case ButtonRelease:
      r_mouse_btn = (ev.type == ButtonPress);
      needs_redraw = 1;
      break;
    case MotionNotify:
      r_mx = ev.xmotion.x;
      r_my = ev.xmotion.y;
      needs_redraw = 1;
      break;
    case KeyPress:
    case KeyRelease: {
      int m = ev.xkey.state;
      KeySym k = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
      for (unsigned int i = 0; i < 124; i += 2) {
        if (KEYCODES[i] == k) {
          r_keys[KEYCODES[i + 1]] = (ev.type == KeyPress);
          break;
        }
      }
      r_mod = (!!(m & ControlMask)) |
        (!!(m & ShiftMask) << 1) |
        (!!(m & Mod1Mask) << 2) |
        (!!(m & Mod4Mask) << 3);

      needs_redraw = 1;
      break;
    }
    case ClientMessage:
      if ((Atom) ev.xclient.data.l[0] == wm_delete_window) r_close_requested = 1;
      break;
    case ConfigureNotify:
      if (ev.xconfigure.width != win_width || ev.xconfigure.height != win_height) {
        r_resize(ev.xconfigure.width, ev.xconfigure.height);
        r_clear(last_clear_color);
        needs_redraw = 1;
      }
      break;
    }
  }
}

/* mouse / keyboard helpers — unchanged */
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
int r_mouse_moved(int * mousex, int * mousey) {
  if (r_mx != * mousex || r_my != * mousey) {
    * mousex = r_mx;* mousey = r_my;
    return 1;
  }
  return 0;
}
int r_should_close(void) {
  return r_close_requested;
}
int r_ctrl_pressed(void) {
  return r_mod & 1;
}
int r_shift_pressed(void) {
  return r_mod & 2;
}
int r_alt_pressed(void) {
  return r_mod & 4;
}
int r_key_down(int key) {
  if (r_keys[key] == 1) {
    r_keys[key]++;
    return 1;
  }
  return 0;
}
int r_key_up(int key) {
  if (r_keys[key] < 1) return 1;
  return 0;
}
int64_t r_get_time(void) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, & t);
  return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
void r_sleep(int64_t ms) {
  struct timespec ts = {
    ms / 1000,
    (ms % 1000) * 1000000
  };
  nanosleep( & ts, NULL);
}
