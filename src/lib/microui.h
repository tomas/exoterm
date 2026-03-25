/*
** Copyright (c) 2020 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `microui.c` for details.
*/

#ifndef MICROUI_H
#define MICROUI_H

#include <time.h> // for timespec

#define MU_VERSION "2.01"

#define MU_COMMANDLIST_SIZE     (256 * 1024)
#define MU_ROOTLIST_SIZE        32
#define MU_CONTAINERSTACK_SIZE  32
#define MU_CLIPSTACK_SIZE       32
#define MU_IDSTACK_SIZE         32
#define MU_LAYOUTSTACK_SIZE     16
#define MU_CONTAINERPOOL_SIZE   48
#define MU_TREENODEPOOL_SIZE    48
#define MU_MAX_WIDTHS           16
#define MU_REAL                 float
#define MU_REAL_FMT             "%.3g"
#define MU_SLIDER_FMT           "%.2f"
#define MU_MAX_FMT              127

#define mu_stack(T, n)          struct { int idx; T items[n]; }
#define mu_min(a, b)            ((a) < (b) ? (a) : (b))
#define mu_max(a, b)            ((a) > (b) ? (a) : (b))
#define mu_clamp(x, a, b)       mu_min(b, mu_max(a, x))

enum {
  MU_CLIP_PART = 1,
  MU_CLIP_ALL
};

enum {
  MU_COMMAND_JUMP = 1,
  MU_COMMAND_CLIP,
  MU_COMMAND_RECT,
  MU_COMMAND_SHAPE,
  MU_COMMAND_TEXT,
  MU_COMMAND_ICON,
  MU_COMMAND_IMAGE,
  MU_COMMAND_MAX
};

enum {
  MU_SHAPE_RECT = 1,
  MU_SHAPE_CIRCLE,
  MU_SHAPE_LINES,
};

enum {
  MU_COLOR_TEXT,
  MU_COLOR_BORDER,
  MU_COLOR_WINDOWBG,
  MU_COLOR_TITLEBG,
  MU_COLOR_TITLETEXT,
  MU_COLOR_PANELBG,
  MU_COLOR_BUTTON,
  MU_COLOR_BUTTONHOVER,
  MU_COLOR_BUTTONFOCUS,
  MU_COLOR_BASE,
  MU_COLOR_BASEHOVER,
  MU_COLOR_BASEFOCUS,
  MU_COLOR_SCROLLBASE,
  MU_COLOR_SCROLLTHUMB,
  MU_COLOR_SCROLLFOCUS,
  MU_COLOR_HANDLE,
  MU_COLOR_HANDLEHOVER,
  MU_COLOR_HANDLEFOCUS,
  MU_COLOR_TABCONTENT,
  MU_COLOR_TABACTIVE,
  MU_COLOR_TABINACTIVE,
  MU_COLOR_TABHOVER,
  MU_COLOR_MAX
};

enum {
  MU_ICON_CLOSE = 1,
  MU_ICON_CHECK,
  MU_ICON_COLLAPSED,
  MU_ICON_EXPANDED,
  MU_ICON_RESIZE_WINDOW,
  MU_ICON_MAX // 6
};

enum {
  MU_RES_ACTIVE       = (1 << 0),
  MU_RES_SUBMIT       = (1 << 1),
  MU_RES_CHANGE       = (1 << 2)
};

enum {
  MU_OPT_ALIGNCENTER  = (1 << 0),
  MU_OPT_ALIGNRIGHT   = (1 << 1),
  MU_OPT_NOINTERACT   = (1 << 2),
  MU_OPT_NOFRAME      = (1 << 3),
  MU_OPT_NORESIZE     = (1 << 4),
  MU_OPT_NOSCROLL     = (1 << 5),
  MU_OPT_NOCLOSE      = (1 << 6),
  MU_OPT_NOTITLE      = (1 << 7),
  MU_OPT_HOLDFOCUS    = (1 << 8),
  MU_OPT_AUTOSIZE     = (1 << 9),
  MU_OPT_POPUP        = (1 << 10),
  MU_OPT_CLOSED       = (1 << 11),
  MU_OPT_EXPANDED     = (1 << 12),
  MU_OPT_NODRAG       = (1 << 13),
  MU_OPT_ALIGNTOP     = (1 << 14),
  MU_OPT_ALIGNBOTTOM  = (1 << 15),
  MU_OPT_RAISED       = (1 << 16),
  MU_OPT_BURIED       = (1 << 17),
  MU_OPT_NOPADDING    = (1 << 18),
  MU_OPT_PREVENT_SCROLL = (1 << 19),
  MU_OPT_HIDE_ON_CLICK  = (1 << 20), // for popups
  MU_OPT_CONTAINHEIGHT  = (1 << 21), // for images
  MU_OPT_ALWAYSONTOP = (1 << 22),
  MU_OPT_CHANGE_ON_RELEASE = (1 << 23), // slider: return MU_RES_CHANGE only on mouse release
};

enum {
  MU_MOUSE_LEFT       = (1 << 0),
  MU_MOUSE_RIGHT      = (1 << 1),
  MU_MOUSE_MIDDLE     = (1 << 2)
};

enum {
  MU_KEY_SHIFT        = (1 << 0),
  MU_KEY_CTRL         = (1 << 1),
  MU_KEY_ALT          = (1 << 2),
  MU_KEY_BACKSPACE    = (1 << 3),
  MU_KEY_RETURN       = (1 << 4),
  MU_KEY_DELETE       = (1 << 5),
  MU_KEY_LEFT         = (1 << 6),
  MU_KEY_RIGHT        = (1 << 7),
  MU_KEY_UP           = (1 << 8),
  MU_KEY_DOWN         = (1 << 9),
  MU_KEY_SPACE        = (1 << 10),
  MU_KEY_HOME         = (1 << 11),
  MU_KEY_END          = (1 << 12),
};

#ifndef uint
typedef unsigned uint;
#endif

#ifndef int32_t
typedef int int32_t;
#endif

typedef struct mu_Context mu_Context;
typedef unsigned mu_Id;
typedef MU_REAL mu_Real;
typedef void* mu_Font;
typedef unsigned int mu_Texture;

typedef struct { int x, y; } mu_Vec2;
typedef struct { int x, y, w, h; } mu_Rect;
typedef struct { float x, y, w, h; } mu_TextureRect;
typedef struct { unsigned char r, g, b, a; } mu_Color;
typedef struct { mu_Id id; int last_update; } mu_PoolItem;
typedef struct { mu_Texture texture; mu_TextureRect rect; uint width; uint height; } mu_Image;

typedef struct { int type, size; } mu_BaseCommand;
typedef struct { mu_BaseCommand base; void *dst; } mu_JumpCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; } mu_ClipCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; mu_Color color; } mu_RectCommand;
typedef struct { mu_BaseCommand base; unsigned char shape; mu_Rect rect; mu_Color color; float scale; float params[10]; } mu_ShapeCommand;
typedef struct { mu_BaseCommand base; mu_Font font; mu_Vec2 pos; mu_Color color; float scale; char str[1]; } mu_TextCommand;
typedef struct { mu_BaseCommand base; mu_Rect rect; int id; mu_Color color; float scale; } mu_IconCommand;
typedef struct { mu_BaseCommand base; mu_Rect dest; mu_Image img; float opacity; float scale; } mu_ImageCommand;

typedef union {
  int type;
  mu_BaseCommand base;
  mu_JumpCommand jump;
  mu_ClipCommand clip;
  mu_ShapeCommand shape;
  mu_RectCommand rect;
  mu_TextCommand text;
  mu_IconCommand icon;
  mu_ImageCommand image;
} mu_Command;

typedef struct {
  mu_Rect body;
  mu_Rect next;
  mu_Vec2 position;
  mu_Vec2 size;
  mu_Vec2 max;
  int widths[MU_MAX_WIDTHS];
  int items;
  int item_index;
  int next_row;
  int next_type;
  int indent;
} mu_Layout;

typedef struct {
  mu_Command *head, *tail;
  mu_Rect rect;
  mu_Rect body;
  mu_Vec2 content_size;
  mu_Vec2 scroll;
  int zindex;
  int zorder;  // 0=normal, 1=always-on-top, 2=topmost (popups)
  int open;
  int opt;
} mu_Container;

typedef struct {
  mu_Font font;
  mu_Vec2 size;
  int padding;
  int spacing;
  int indent;
  int title_height;
  int scrollbar_size;
  int thumb_size;
  mu_Color colors[MU_COLOR_MAX];
} mu_Style;

struct mu_Context {
  /* callbacks */
  int (*text_width)(mu_Font font, const char *str, int len);
  int (*font_height)(mu_Font font);
  int (*line_height)(mu_Font font);
  void (*draw_frame)(mu_Context *ctx, mu_Rect rect, int colorid);
  /* core state */
  mu_Style _style;
  mu_Style *style;
  mu_Id hover;
  mu_Id focus;
  mu_Id last_focus;
  mu_Id last_id;
  mu_Rect last_rect;
  int hover_type;
  int last_zindex;
  int updated_focus;
  int frame;
  int needs_redraw;
  mu_Container *hover_root;
  mu_Container *next_hover_root;
  mu_Container *scroll_target;
  char number_edit_buf[MU_MAX_FMT];
  mu_Id number_edit;
  /* stacks */
  mu_stack(char, MU_COMMANDLIST_SIZE) command_list;
  mu_stack(mu_Container*, MU_ROOTLIST_SIZE) root_list;
  mu_stack(mu_Container*, MU_CONTAINERSTACK_SIZE) container_stack;
  mu_stack(mu_Rect, MU_CLIPSTACK_SIZE) clip_stack;
  mu_stack(mu_Id, MU_IDSTACK_SIZE) id_stack;
  mu_stack(mu_Layout, MU_LAYOUTSTACK_SIZE) layout_stack;
  /* retained state pools */
  mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE];
  mu_Container containers[MU_CONTAINERPOOL_SIZE];
  mu_PoolItem treenode_pool[MU_TREENODEPOOL_SIZE];
  /* input state */
  mu_Vec2 mouse_pos;
  mu_Vec2 last_mouse_pos;
  mu_Vec2 mouse_delta;
  mu_Vec2 scroll_delta;
  int prevent_scroll;
  int mouse_down;
  int mouse_pressed;
  int mouse_up;
  int scrollbar_drag;
  int key_down;
  int key_pressed;
#ifdef __linux__
  struct timespec mouse_down_ts;
#else
  struct timeval mouse_down_ts;
#endif
  double last_click_time;
  char input_text[64];
  char copy_text[64];
  int textbox_index;
  int textbox_select_min;
  int textbox_select_max;
};

mu_Vec2 mu_vec2(int x, int y);
mu_Rect mu_rect(int x, int y, int w, int h);
mu_Color mu_color(int r, int g, int b, int a);

void mu_init(mu_Context *ctx);
void mu_begin(mu_Context *ctx);
void mu_end(mu_Context *ctx);
void mu_set_focus(mu_Context *ctx, mu_Id id);
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size);
void mu_push_id(mu_Context *ctx, const void *data, int size);
void mu_pop_id(mu_Context *ctx);
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect);
void mu_pop_clip_rect(mu_Context *ctx);
mu_Rect mu_get_clip_rect(mu_Context *ctx);
int mu_check_clip(mu_Context *ctx, mu_Rect r);
mu_Container* mu_get_current_container(mu_Context *ctx);
mu_Container* mu_get_container(mu_Context *ctx, const char *name);
void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt);

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id);
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx);

void mu_input_mousemove(mu_Context *ctx, int x, int y);
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn);
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn);
void mu_input_scroll(mu_Context *ctx, int x, int y);
void mu_input_keydown(mu_Context *ctx, int key);
void mu_input_keyup(mu_Context *ctx, int key);
void mu_input_text(mu_Context *ctx, const char *text);

mu_Command* mu_push_command(mu_Context *ctx, int type, int size);
int mu_next_command(mu_Context *ctx, mu_Command **cmd);
void mu_set_clip(mu_Context *ctx, mu_Rect rect);
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color);
void mu_draw_box_shadow(mu_Context *ctx, mu_Rect rect, mu_Color color, float opacity, uint roundness, uint width, int offset_x, int offset_y);
void mu_draw_rounded_box(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness);
void mu_draw_rounded_rect(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness);
void mu_draw_box_inner(mu_Context *ctx, mu_Rect rect, mu_Color color, uint width, uint roundness);
void mu_draw_box_outer(mu_Context *ctx, mu_Rect rect, mu_Color color, uint width, uint roundness);
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color);
void mu_draw_text_scaled(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color, float scale);
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color);
void mu_draw_icon_scaled(mu_Context *ctx, int id, mu_Rect rect, mu_Color color, float scale);

// void mu_image_init(mu_Image* image);
// void mu_image_update(mu_Image* image, int size_x, int size_y, uint32_t* data);
mu_Image mu_new_image(mu_Texture texture, unsigned int width, unsigned int height);
mu_Image mu_sub_image(mu_Image image, mu_Rect rect);

void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height);
void mu_layout_width(mu_Context *ctx, int width);
void mu_layout_height(mu_Context *ctx, int height);
void mu_layout_begin_column(mu_Context *ctx);
void mu_layout_end_column(mu_Context *ctx);
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative);
mu_Rect mu_layout_next(mu_Context *ctx);

void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect, int colorid, int opt);
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect, int colorid, int opt, int offset_x, int offset_y);
int mu_mouse_over(mu_Context *ctx, mu_Rect rect);
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt);

#define mu_button(ctx, label)             mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER)
#define mu_textbox(ctx, buf, bufsz)       mu_textbox_ex(ctx, buf, bufsz, 0)
#define mu_slider(ctx, value, lo, hi)     mu_slider_ex(ctx, value, lo, hi, 0, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_number(ctx, value, step)       mu_number_ex(ctx, value, step, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_header(ctx, label)             mu_header_ex(ctx, label, 0)
#define mu_begin_treenode(ctx, label)     mu_begin_treenode_ex(ctx, label, 0)
#define mu_begin_window(ctx, title, rect) mu_begin_window_ex(ctx, title, rect, 0)
#define mu_begin_panel(ctx, name)         mu_begin_panel_ex(ctx, name, 0)
#define mu_begin_combo(ctx, id, item, max)  mu_begin_combo_ex(cts, id, item, max, 0x00)

void mu_image_ex(mu_Context *ctx, mu_Image image);
void mu_text_ex(mu_Context *ctx, const char *text, mu_Font font);
void mu_text(mu_Context *ctx, const char *text);
void mu_label(mu_Context *ctx, const char *text, int opt);
void mu_label_ex(mu_Context *ctx, const char *text, mu_Color color, int xoff, int yoff, float scale, int opt);
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt);
int mu_color_button_ex(mu_Context *ctx, const char *label, mu_Color color, int opt);
int mu_checkbox(mu_Context *ctx, const char *label, int *state);
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt);
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt);
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high, mu_Real step, const char *fmt, int opt);
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt);
int mu_header_ex(mu_Context *ctx, const char *label, int opt);
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt);
void mu_end_treenode(mu_Context *ctx);
void mu_set_window_rect(mu_Context *ctx, const char *title, mu_Rect rect);
int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt);
void mu_end_window(mu_Context *ctx);
void mu_open_popup(mu_Context *ctx, const char *name);
void mu_close_popup(mu_Context *ctx, const char *name);
int mu_begin_popup(mu_Context *ctx, const char *name, int opt);
int mu_begin_popup_ex(mu_Context *ctx, const char *name, mu_Rect rect, int opt);
void mu_end_popup(mu_Context *ctx);
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt);
void mu_end_panel(mu_Context *ctx);

int mu_begin_combo_ex(mu_Context* ctx, const char* id, const char* current_item, int32_t max_items, int32_t opt);
void mu_end_combo(mu_Context* ctx);

void mu_menu_separator(mu_Context *ctx);

// void mu_begin_tabs(mu_Context *ctx, const char *name, int opt);
int mu_begin_tabs(mu_Context *ctx, const char *name, int num_tabs, const char **tab_names, int *active_tab);
void mu_end_tabs(mu_Context *ctx);
int mu_add_tab(mu_Context *ctx, const char *label, int opt, int expanded);

#endif
