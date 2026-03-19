#ifndef RENDERER_H
#define RENDERER_H

#include "microui.h"
#include <X11/Xlib.h>
#include <stdint.h>

void r_init(Display *display, Window window, GC context, Visual *visual, int depth, int width, int height);
/* Update the display/window/GC without touching the pixel buffer or image. */
void r_update_context(Display *display, Window window, GC context);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
 int r_get_text_width(const char *text, int len);
 int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
void r_clear(mu_Color color);
void r_present(void);
void r_present_noevents(void);
void r_resize(int width, int height);
// Can only be checked once per frame; side-effecting.
 int r_mouse_down(void);
// Can only be checked once per frame; side-effecting.
 int r_mouse_up(void);
// Can only be checked once per frame; side-effecting.
 int r_mouse_moved(int *x, int *y);
// Can only be checked once per key per frame; side-effecting.
 int r_key_down(int key);
 int r_key_up(int key);
 int r_ctrl_pressed(void);
 int r_shift_pressed(void);
 int r_alt_pressed(void);
 int r_should_close(void);
 int64_t r_get_time(void);
 void r_sleep(int64_t ms);

#endif

