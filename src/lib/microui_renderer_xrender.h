#ifndef MICROUI_RENDERER_H
#define MICROUI_RENDERER_H

#include <X11/Xlib.h>
#include <stdint.h>
#include "microui.h"

int r_init(Display *display, Window window, GC context, Visual *visual, int depth, int width, int height);
void r_update_context(Display *display, Window window, GC context);
void r_resize(int width, int height);
void r_clear(mu_Color clr);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
int r_get_text_width(const char *text, int len);
int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
void r_present(void);
void r_present_noevents(void);

/* Input */
int r_mouse_down(void);
int r_mouse_up(void);
int r_mouse_moved(int *mousex, int *mousey);
int r_should_close(void);
int r_needs_redraw(void);  /* New function for expose handling */
int r_ctrl_pressed(void);
int r_shift_pressed(void);
int r_alt_pressed(void);
int r_key_down(int key);
int r_key_up(int key);

/* Timing */
int64_t r_get_time(void);
void r_sleep(int64_t ms);

#endif