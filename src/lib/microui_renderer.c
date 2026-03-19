#define _DEFAULT_SOURCE 1
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "microui_renderer.h"
#include "atlas.inl"

#define BUFFER_SIZE 16384

typedef uint8_t byte;

static mu_Rect tex_buf[BUFFER_SIZE];
static mu_Rect src_buf[BUFFER_SIZE];
static mu_Color color_buf[BUFFER_SIZE];

static int buf_idx;

/* X11 state */
static Display  *dpy;
static Window    win;
static GC        gc;
static Visual   *s_visual;
static int       s_depth;
static XImage   *img;
static uint32_t *buf;
static int win_width;
static int win_height;

/* Input state */
static int r_keys[256];
static int r_mod;
static int r_mx;
static int r_my;
static int r_mouse_btn;
static int r_close_requested;
static Atom wm_delete_window;

static mu_Rect clip_rect;

/* clang-format off */
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
/* clang-format on */

void r_init(Display *display, Window window, GC context, Visual *visual, int depth, int width, int height) {
  dpy        = display;
  win        = window;
  gc         = context;
  s_visual   = visual;
  s_depth    = depth;
  win_width  = width;
  win_height = height;

  buf = malloc(width * height * sizeof(*buf));

  XSelectInput(dpy, win,
               ExposureMask | KeyPressMask | KeyReleaseMask |
               ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
               StructureNotifyMask);

  img = XCreateImage(dpy, s_visual, s_depth, ZPixmap, 0, (char *)buf, width, height, 32, 0);

  clip_rect = mu_rect(0, 0, width, height);

  wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wm_delete_window, 1);

  r_clear(mu_color(0, 0, 0, 255));
}

void r_update_context(Display *display, Window window, GC context) {
  dpy = display;
  win = window;
  gc  = context;
}

void r_resize(int width, int height) {
  /* XDestroyImage would free img->data (our buf), so clear it first */
  img->data = NULL;
  XDestroyImage(img);
  free(buf);

  win_width  = width;
  win_height = height;
  buf = malloc(width * height * sizeof(*buf));

  img = XCreateImage(dpy, s_visual, s_depth, ZPixmap, 0, (char *)buf, width, height, 32, 0);

  clip_rect = mu_rect(0, 0, width, height);
}

static inline bool within(int c, int lo, int hi) {
  return c >= lo && c < hi;
}

static inline bool within_rect(mu_Rect rect, int x, int y) {
  return within(x, rect.x, rect.x + rect.w)
      && within(y, rect.y, rect.y + rect.h);
}

static inline bool same_size(const mu_Rect *a, const mu_Rect *b) {
  return a->w == b->w && a->h == b->h;
}

static inline byte texture_color(const mu_Rect *tex, int x, int y) {
  assert(x < tex->w);
  x = x + tex->x;
  assert(y < tex->h);
  y = y + tex->y;
  return atlas_texture[y * ATLAS_WIDTH + x];
}

static inline uint32_t r_color(mu_Color clr) {
  return ((uint32_t)clr.a << 24) | ((uint32_t)clr.r << 16)
       | ((uint32_t)clr.g << 8)  |  (uint32_t)clr.b;
}

mu_Color mu_color_argb(uint32_t clr) {
  return mu_color((clr >> 16) & 0xff, (clr >> 8) & 0xff,
                   clr & 0xff, (clr >> 24) & 0xff);
}

static inline int greyscale(byte c) {
  return r_color(mu_color(c, c, c, 255));
}

static inline mu_Color blend_pixel(mu_Color dst, mu_Color src) {
  int ia = 0xff - src.a;
  dst.r = ((src.r * src.a) + (dst.r * ia)) >> 8;
  dst.g = ((src.g * src.a) + (dst.g * ia)) >> 8;
  dst.b = ((src.b * src.a) + (dst.b * ia)) >> 8;
  return dst;
}

#define pixel(x, y) buf[((y) * win_width) + (x)]

static void flush(void) {
  for (int i = 0; i < buf_idx; i++) {
    mu_Rect *src = &src_buf[i];
    mu_Rect *tex = &tex_buf[i];
    int ystart = mu_max(src->y, clip_rect.y);
    int yend   = mu_min(src->y + src->h, clip_rect.y + clip_rect.h);
    int xstart = mu_max(src->x, clip_rect.x);
    int xend   = mu_min(src->x + src->w, clip_rect.x + clip_rect.w);
    if (same_size(src, tex)) {
      for (int y = ystart; y < yend; y++) {
        for (int x = xstart; x < xend; x++) {
          assert(within_rect(*src, x, y));
          assert(within_rect(clip_rect, x, y));
          byte tc = texture_color(tex, x - src->x, y - src->y);
          pixel(x, y) |= greyscale(tc);
        }
      }
    } else {
      mu_Color new_color = color_buf[i];
      for (int y = ystart; y < yend; y++) {
        for (int x = xstart; x < xend; x++) {
          assert(within_rect(*src, x, y));
          assert(within_rect(clip_rect, x, y));
          mu_Color existing = mu_color_argb(pixel(x, y));
          pixel(x, y) = r_color(blend_pixel(existing, new_color));
        }
      }
    }
  }
  buf_idx = 0;
}

static void push_quad(mu_Rect src, mu_Rect tex, mu_Color color) {
  if (buf_idx == BUFFER_SIZE) { flush(); }
  tex_buf[buf_idx]   = tex;
  src_buf[buf_idx]   = src;
  color_buf[buf_idx] = color;
  buf_idx++;
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
  push_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  mu_Rect dst = { pos.x, pos.y, 0, 0 };
  for (const char *p = text; *p; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char)*p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;
    push_quad(dst, src, color);
    dst.x += dst.w;
  }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;
  push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

int r_get_text_width(const char *text, int len) {
  int res = 0;
  for (const char *p = text; *p && len--; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char)*p, 127);
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}

int r_get_text_height(void) {
  return 18;
}

void r_set_clip_rect(mu_Rect rect) {
  flush();
  int ystart = mu_max(0, rect.y);
  int yend   = mu_min(win_height, rect.y + rect.h);
  int xstart = mu_max(0, rect.x);
  int xend   = mu_min(win_width,  rect.x + rect.w);
  clip_rect  = mu_rect(xstart, ystart, xend - xstart, yend - ystart);
}

void r_clear(mu_Color clr) {
  flush();
  uint32_t c = r_color(clr);
  for (int i = 0; i < win_width * win_height; i++) {
    buf[i] = c;
  }
}

static void process_events(void) {
  XEvent ev;
  while (XPending(dpy)) {
    XNextEvent(dpy, &ev);
    switch (ev.type) {
    case ButtonPress:
    case ButtonRelease:
      r_mouse_btn = (ev.type == ButtonPress);
      break;
    case MotionNotify:
      r_mx = ev.xmotion.x;
      r_my = ev.xmotion.y;
      break;
    case KeyPress:
    case KeyRelease: {
      int m = ev.xkey.state;
      int k = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
      for (unsigned int i = 0; i < 124; i += 2) {
        if (KEYCODES[i] == k) {
          r_keys[KEYCODES[i + 1]] = (ev.type == KeyPress);
          break;
        }
      }
      r_mod = (!!(m & ControlMask))       |
              (!!(m & ShiftMask)    << 1)  |
              (!!(m & Mod1Mask)     << 2)  |
              (!!(m & Mod4Mask)     << 3);
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

void r_present(void) {
  flush();
  XPutImage(dpy, win, gc, img, 0, 0, 0, 0, win_width, win_height);
  XFlush(dpy);
  process_events();
}

/* Like r_present but without draining the X event queue.
   Use this when the host application handles its own event dispatch. */
void r_present_noevents(void) {
  flush();
  XPutImage(dpy, win, gc, img, 0, 0, 0, 0, win_width, win_height);
  XFlush(dpy);
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

int r_ctrl_pressed(void)  { return r_mod & 1; }
int r_shift_pressed(void) { return r_mod & 2; }
int r_alt_pressed(void)   { return r_mod & 4; }

int r_key_down(int key) {
  if (r_keys[key] == 1) {
    r_keys[key]++;
    return 1;
  }
  return 0;
}

int r_key_up(int key) {
  if (r_keys[key] < 1) {
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
