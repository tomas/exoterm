/*----------------------------------------------------------------------*
 * File:        settings_ui.C - microui-based settings side panel
 *----------------------------------------------------------------------*/

#include "../config.h"
#include "rxvt.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern "C" {
#include "lib/microui.h"
#include "lib/microui_renderer.h"
}

#include <X11/extensions/Xrender.h>

/* Panel sits flush on the right edge, full parent height. */
#define PANEL_WIDTH 320

/* Current panel height — set dynamically to the parent window height. */
static int s_panel_h = 600;

/* --- microui context --- */
static mu_Context *mu_ctx = nullptr;
static int s_mousex = 0, s_mousey = 0;

/* --- live settings state --- */
static float s_border_width  = 10.0f;
static float s_scroll_speed  = 5.0f;
static float s_shading       = 20.0f;
static float s_line_space    = 0.0f;
static float s_letter_space  = 0.0f;
static float s_save_lines    = 1000.0f;
static int   s_cursor_blink       = 0;
static int   s_cursor_underline   = 0;
static int   s_scrollbar          = 1;
static int   s_scroll_on_output   = 0;
static int   s_scroll_on_keypress = 1;
static int   s_jump_scroll        = 1;
static int   s_visual_bell        = 0;
static int   s_urgent_on_bell     = 0;
static int   s_mouse_wheel_page   = 0;
static int   s_pointer_blank      = 0;
static int   s_active_scheme      = -1;
static int   s_pending_scheme     = -1;
static int   s_active_font        = -1;
static int   s_pending_font       = -1;

/* ======================================================================
   Snapshot — saved on open, restored on Cancel
   ====================================================================== */
struct settings_snapshot_t {
  float border_width, scroll_speed, shading, line_space, letter_space;
  float save_lines;
  int   cursor_blink, cursor_underline, scrollbar;
  int   scroll_on_output, scroll_on_keypress, jump_scroll;
  int   visual_bell, urgent_on_bell, mouse_wheel_page, pointer_blank;
  char  colors[18][8]; /* "#rrggbb\0" for fg, bg, ANSI 0-15 */
  char *font;          /* owned copy of rs[Rs_font], may be NULL */
  int   active_scheme, active_font;
};

static settings_snapshot_t s_snapshot = {};

static void color_to_hex (rxvt_term *t, int idx, char *dst7) {
  rgba c = (rgba) t->pix_colors_focused[idx];
  snprintf (dst7, 8, "#%02x%02x%02x", c.r >> 8, c.g >> 8, c.b >> 8);
}

static void save_snapshot (rxvt_term *t) {
  s_snapshot.border_width       = s_border_width;
  s_snapshot.scroll_speed       = s_scroll_speed;
  s_snapshot.shading            = s_shading;
  s_snapshot.line_space         = s_line_space;
  s_snapshot.letter_space       = s_letter_space;
  s_snapshot.save_lines         = s_save_lines;
  s_snapshot.cursor_blink       = s_cursor_blink;
  s_snapshot.cursor_underline   = s_cursor_underline;
  s_snapshot.scrollbar          = s_scrollbar;
  s_snapshot.scroll_on_output   = s_scroll_on_output;
  s_snapshot.scroll_on_keypress = s_scroll_on_keypress;
  s_snapshot.jump_scroll        = s_jump_scroll;
  s_snapshot.visual_bell        = s_visual_bell;
  s_snapshot.urgent_on_bell     = s_urgent_on_bell;
  s_snapshot.mouse_wheel_page   = s_mouse_wheel_page;
  s_snapshot.pointer_blank      = s_pointer_blank;
  s_snapshot.active_scheme      = s_active_scheme;
  s_snapshot.active_font        = s_active_font;

  color_to_hex (t, Color_fg, s_snapshot.colors[0]);
  color_to_hex (t, Color_bg, s_snapshot.colors[1]);
  for (int i = 0; i < 16; i++)
    color_to_hex (t, minCOLOR + i, s_snapshot.colors[2 + i]);

  free (s_snapshot.font);
  s_snapshot.font = t->rs[Rs_font] ? strdup (t->rs[Rs_font]) : nullptr;
}

/* ======================================================================
   Color schemes
   palette[0]  = fg,  palette[1] = bg
   palette[2]  = ANSI 0  → minCOLOR + 0
   palette[17] = ANSI 15 → minCOLOR + 15
   ====================================================================== */
struct color_scheme_t {
  const char *name;
  const char *palette[18];
};

static const color_scheme_t color_schemes[] = {
  { "Dracula", {
    "#f8f8f2", "#282a36",
    "#21222c", "#ff5555", "#50fa7b", "#f1fa8c",
    "#bd93f9", "#ff79c6", "#8be9fd", "#f8f8f2",
    "#6272a4", "#ff6e6e", "#69ff94", "#ffffa5",
    "#d6acff", "#ff92df", "#a4ffff", "#ffffff",
  }},
  { "Gruvbox Dark", {
    "#ebdbb2", "#282828",
    "#282828", "#cc241d", "#98971a", "#d79921",
    "#458588", "#b16286", "#689d6a", "#a89984",
    "#928374", "#fb4934", "#b8bb26", "#fabd2f",
    "#83a598", "#d3869b", "#8ec07c", "#ebdbb2",
  }},
  { "Nord", {
    "#d8dee9", "#2e3440",
    "#3b4252", "#bf616a", "#a3be8c", "#ebcb8b",
    "#81a1c1", "#b48ead", "#88c0d0", "#e5e9f0",
    "#4c566a", "#bf616a", "#a3be8c", "#ebcb8b",
    "#81a1c1", "#b48ead", "#8fbcbb", "#eceff4",
  }},
  { "Solarized Dark", {
    "#839496", "#002b36",
    "#073642", "#dc322f", "#859900", "#b58900",
    "#268bd2", "#d33682", "#2aa198", "#eee8d5",
    "#002b36", "#cb4b16", "#586e75", "#657b83",
    "#839496", "#6c71c4", "#93a1a1", "#fdf6e3",
  }},
  { "One Dark", {
    "#abb2bf", "#282c34",
    "#282c34", "#e06c75", "#98c379", "#e5c07b",
    "#61afef", "#c678dd", "#56b6c2", "#abb2bf",
    "#5c6370", "#e06c75", "#98c379", "#e5c07b",
    "#61afef", "#c678dd", "#56b6c2", "#ffffff",
  }},
  { "Tokyo Night", {
    "#a9b1d6", "#1a1b26",
    "#32344a", "#f7768e", "#9ece6a", "#e0af68",
    "#7aa2f7", "#ad8ee6", "#449dab", "#787c99",
    "#444b6a", "#ff7a93", "#b9f27c", "#ff9e64",
    "#7da6ff", "#bb9af7", "#0db9d7", "#acb0d0",
  }},
  { "Catppuccin Mocha", {
    "#cdd6f4", "#1e1e2e",
    "#45475a", "#f38ba8", "#a6e3a1", "#f9e2af",
    "#89b4fa", "#f5c2e7", "#94e2d5", "#bac2de",
    "#585b70", "#f38ba8", "#a6e3a1", "#f9e2af",
    "#89b4fa", "#f5c2e7", "#94e2d5", "#a6adc8",
  }},
  { "Monokai", {
    "#f8f8f2", "#272822",
    "#272822", "#f92672", "#a6e22e", "#f4bf75",
    "#66d9e8", "#ae81ff", "#a1efe4", "#f8f8f2",
    "#75715e", "#f92672", "#a6e22e", "#f4bf75",
    "#66d9e8", "#ae81ff", "#a1efe4", "#f9f8f5",
  }},
};

static const int NUM_SCHEMES = (int)(sizeof (color_schemes) / sizeof (color_schemes[0]));

/* ======================================================================
   Font list  (replace with real bitmap-font entries later)
   ====================================================================== */
struct font_entry_t {
  const char *name;
  const char *spec;
};

static const font_entry_t font_entries[] = {
  { "Cozette 13px",          "-*-cozette-medium-r-normal-*-13-*-*-*-*-*-*-*"                },
  { "Terminus 12px",         "-*-terminus-medium-r-normal-*-12-*-*-*-*-*-iso8859-1"         },
  { "Terminus 14px",         "-*-terminus-medium-r-normal-*-14-*-*-*-*-*-iso8859-1"         },
  { "Terminus 16px",         "-*-terminus-medium-r-normal-*-16-*-*-*-*-*-iso8859-1"         },
  { "Terminus Bold 16px",    "-*-terminus-bold-r-normal-*-16-*-*-*-*-*-iso8859-1"           },
  { "Misc Fixed 13px",       "-misc-fixed-medium-r-normal-*-13-*-*-*-c-*-iso10646-1"        },
  { "Misc Fixed 14px",       "-misc-fixed-medium-r-normal-*-14-*-*-*-c-*-iso10646-1"        },
  { "Misc Fixed 15px",       "-misc-fixed-medium-r-semicondensed-*-15-*-*-*-c-*-iso10646-1" },
  { "Misc Fixed 18px",       "-misc-fixed-medium-r-normal-*-18-*-*-*-c-*-iso10646-1"        },
  { "Spleen 8x16",           "-*-spleen-medium-r-normal-*-16-*-*-*-c-80-*-*"               },
  { "Spleen 12x24",          "-*-spleen-medium-r-normal-*-24-*-*-*-c-120-*-*"              },
  { "Spleen 16x32",          "-*-spleen-medium-r-normal-*-32-*-*-*-c-160-*-*"              },
  { "GohuFont 11px",         "-*-gohufont-medium-r-normal-*-11-*-*-*-c-*-iso10646-1"        },
  { "GohuFont 14px",         "-*-gohufont-medium-r-normal-*-14-*-*-*-c-*-iso10646-1"        },
  { "Dina 8px",              "-*-dina-medium-r-normal-*-8-*-*-*-*-*-iso8859-1"              },
  { "Dina 9px",              "-*-dina-medium-r-normal-*-9-*-*-*-*-*-iso8859-1"              },
  { "Scientifica 11px",      "-*-scientifica-medium-r-normal-*-11-*-*-*-c-*-iso10646-1"     },
};

static const int NUM_FONTS = (int)(sizeof (font_entries) / sizeof (font_entries[0]));

/* --- microui callbacks --- */
static int text_width_cb (mu_Font font, const char *text, int len) {
  if (len == -1) len = strlen (text);
  return r_get_text_width (text, len);
}
static int font_height_cb (mu_Font font) { return r_get_text_height (); }

/* --- slider helper --- */
static int float_slider (mu_Context *ctx, float *value, float lo, float hi,
                         float step, const char *fmt) {
  mu_push_id (ctx, &value, sizeof (value));
  int res = mu_slider_ex (ctx, value, lo, hi, step, fmt, MU_OPT_ALIGNCENTER);
  mu_pop_id (ctx);
  return res;
}

/* --- hex "#rrggbb" → mu_Color --- */
static mu_Color hex_to_mu (const char *hex) {
  if (!hex || !hex[0]) return mu_color (80, 80, 80, 255);
  if (hex[0] == '#') hex++;
  unsigned r = 0, g = 0, b = 0;
  sscanf (hex, "%02x%02x%02x", &r, &g, &b);
  return mu_color ((int)r, (int)g, (int)b, 255);
}

/* Bitmask of which settings changed this frame */
enum {
  CHANGED_BORDER         = 1 << 0,
  CHANGED_SCROLL         = 1 << 1,
  CHANGED_CURSOR_BLINK   = 1 << 2,
  CHANGED_SHADING        = 1 << 3,
  CHANGED_LINE_SPACE     = 1 << 4,
  CHANGED_LETTER_SPACE   = 1 << 5,
  CHANGED_SAVE_LINES     = 1 << 6,
  CHANGED_CURSOR_UL      = 1 << 8,
  CHANGED_SCROLLBAR      = 1 << 9,
  CHANGED_SCROLL_OUTPUT  = 1 << 10,
  CHANGED_SCROLL_KEY     = 1 << 11,
  CHANGED_JUMP_SCROLL    = 1 << 12,
  CHANGED_VISUAL_BELL    = 1 << 13,
  CHANGED_URGENT_BELL    = 1 << 14,
  CHANGED_WHEEL_PAGE     = 1 << 15,
  CHANGED_PTR_BLANK      = 1 << 16,
  CHANGED_COLOR_SCHEME   = 1 << 17,
  CHANGED_FONT           = 1 << 18,
  CHANGED_APPLY          = 1 << 29,
  CHANGED_CANCEL         = 1 << 30,
};

/* --- section header --- */
static void section_header (mu_Context *ctx, const char *label) {
  { int c[] = {-1}; mu_layout_row (ctx, 1, c, 6); }
  mu_label (ctx, "", 0);
  { int c[] = {-1}; mu_layout_row (ctx, 1, c, 20); }
  mu_label (ctx, label, 0);
}

/* --- draw one color scheme row, return true if clicked --- */
static bool draw_scheme_row (mu_Context *ctx, int idx) {
  const color_scheme_t &s = color_schemes[idx];
  bool active = (s_active_scheme == idx);

  { int c[] = {-1}; mu_layout_row (ctx, 1, c, 22); }
  char label[48];
  snprintf (label, sizeof (label), "%s%s", active ? ">  " : "   ", s.name);
  bool clicked = (bool) mu_button (ctx, label);

  /* 16-color swatch strip */
  int sw = (PANEL_WIDTH - 30) / 16;
  { int widths[16]; for (int i = 0; i < 16; i++) widths[i] = sw;
    mu_layout_row (ctx, 16, widths, 10); }
  for (int i = 0; i < 16; i++) {
    mu_Rect r = mu_layout_next (ctx);
    mu_draw_rect (ctx, r, hex_to_mu (s.palette[2 + i]));
  }
  return clicked;
}

/* --- build settings panel, return CHANGED_* bitmask --- */
static int build_settings_window (mu_Context *ctx) {
  int changed = 0;
  mu_Rect rect = mu_rect (0, 0, PANEL_WIDTH, s_panel_h);

  int opt = MU_OPT_NOCLOSE | MU_OPT_NORESIZE | MU_OPT_NODRAG; //  | MU_OPT_NOINTERACT;

  if (mu_begin_window_ex (ctx, "Settings", rect, opt)) {
    int lw = 120; /* label column width — tuned for PANEL_WIDTH 320 */

    // set container to match the window's height
    mu_Container *win = mu_get_current_container(ctx);
    win->rect.h = s_panel_h;

    /* ---- Appearance ---- */
    section_header (ctx, "Appearance");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    mu_label (ctx, "Shading:", 0);
    if (float_slider (ctx, &s_shading, 0.0f, 100.0f, 1.0f, "%.0f %%") & MU_RES_CHANGE)
      changed |= CHANGED_SHADING;
    mu_label (ctx, "Border:", 0);
    if (float_slider (ctx, &s_border_width, 0.0f, 30.0f, 1.0f, "%.0f px") & MU_RES_CHANGE)
      changed |= CHANGED_BORDER;
    mu_label (ctx, "Line spacing:", 0);
    if (float_slider (ctx, &s_line_space, -4.0f, 16.0f, 1.0f, "%.0f px") & MU_RES_CHANGE)
      changed |= CHANGED_LINE_SPACE;

    // mu_label (ctx, "Letter spacing:", 0);
    // if (float_slider (ctx, &s_letter_space, -4.0f, 8.0f, 1.0f, "%.0f px") & MU_RES_CHANGE)
    //   changed |= CHANGED_LETTER_SPACE;

    /* ---- Fonts ---- */
    section_header (ctx, "Font");
    { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }

    const char *current = s_active_font == -1 ? "Choose one" : font_entries[s_active_font].name;
    if (mu_begin_combo_ex(ctx, "##fonts", current, NUM_FONTS, 0)) {
      { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }
      for (int i = 0; i < NUM_FONTS; i++) {
        char label[64];
        snprintf (label, sizeof (label), "%s%s", (s_active_font == i) ? ">  " : "   ", font_entries[i].name);
        if (mu_button(ctx, label)) {
          s_pending_font = i;
          changed |= CHANGED_FONT;
          mu_close_popup(ctx, "##fonts");
        }
      }
      mu_end_combo(ctx);
    }

    /* ---- Cursor ---- */
    section_header (ctx, "Cursor");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    mu_label (ctx, "Blink:", 0);
    if (mu_checkbox (ctx, "##cblink", &s_cursor_blink) & MU_RES_CHANGE)
      changed |= CHANGED_CURSOR_BLINK;
    mu_label (ctx, "Underline:", 0);
    if (mu_checkbox (ctx, "##cul", &s_cursor_underline) & MU_RES_CHANGE)
      changed |= CHANGED_CURSOR_UL;

    /* ---- Scrolling ---- */
    section_header (ctx, "Scrolling");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    mu_label (ctx, "Speed:", 0);
    if (float_slider (ctx, &s_scroll_speed, 1.0f, 20.0f, 1.0f, "%.0f lines") & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL;
    mu_label (ctx, "Scrollback:", 0);
    if (float_slider (ctx, &s_save_lines, 100.0f, 50000.0f, 100.0f, "%.0f") & MU_RES_CHANGE)
      changed |= CHANGED_SAVE_LINES;

    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    mu_label (ctx, "Scrollbar:", 0);
    if (mu_checkbox (ctx, "##sbar", &s_scrollbar) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLLBAR;
    mu_label (ctx, "Jump scroll:", 0);
    if (mu_checkbox (ctx, "##jscroll", &s_jump_scroll) & MU_RES_CHANGE)
      changed |= CHANGED_JUMP_SCROLL;
    mu_label (ctx, "On output:", 0);
    if (mu_checkbox (ctx, "##sout", &s_scroll_on_output) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL_OUTPUT;
    mu_label (ctx, "On keypress:", 0);
    if (mu_checkbox (ctx, "##skey", &s_scroll_on_keypress) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL_KEY;
    mu_label (ctx, "Page on wheel:", 0);
    if (mu_checkbox (ctx, "##wpage", &s_mouse_wheel_page) & MU_RES_CHANGE)
      changed |= CHANGED_WHEEL_PAGE;

    /* ---- Alerts & misc ---- */
    section_header (ctx, "Alerts & Misc");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    mu_label (ctx, "Visual bell:", 0);
    if (mu_checkbox (ctx, "##vbell", &s_visual_bell) & MU_RES_CHANGE)
      changed |= CHANGED_VISUAL_BELL;
    mu_label (ctx, "Urgent on bell:", 0);
    if (mu_checkbox (ctx, "##urgbell", &s_urgent_on_bell) & MU_RES_CHANGE)
      changed |= CHANGED_URGENT_BELL;
    mu_label (ctx, "Blank pointer:", 0);
    if (mu_checkbox (ctx, "##pblank", &s_pointer_blank) & MU_RES_CHANGE)
      changed |= CHANGED_PTR_BLANK;


    /* ---- Color Schemes ---- */
    section_header (ctx, "Color Schemes");
    for (int i = 0; i < NUM_SCHEMES; i++) {
      if (draw_scheme_row (ctx, i)) {
        s_pending_scheme = i;
        changed |= CHANGED_COLOR_SCHEME;
      }
    }

    /* ---- Buttons ---- */
    { int c[] = {-1}; mu_layout_row (ctx, 1, c, 10); }
    mu_label (ctx, "", 0);
    { int c[] = {-160, -80, -1}; mu_layout_row (ctx, 3, c, 0); }
    mu_label (ctx, "", 0);
    if (mu_button (ctx, "Cancel")) changed |= CHANGED_CANCEL;
    if (mu_button (ctx, "Apply"))  changed |= CHANGED_APPLY;

    mu_end_window (ctx);
  }

  return changed;
}

/* --- render microui command list --- */
static void render_mu (mu_Context *ctx) {
  mu_Command *cmd = nullptr;
  while (mu_next_command (ctx, &cmd)) {
    switch (cmd->type) {
      case MU_COMMAND_TEXT: r_draw_text (cmd->text.str, cmd->text.pos, cmd->text.color); break;
      case MU_COMMAND_RECT: r_draw_rect (cmd->rect.rect, cmd->rect.color);               break;
      case MU_COMMAND_ICON: r_draw_icon (cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
      case MU_COMMAND_CLIP: r_set_clip_rect (cmd->clip.rect);                            break;
    }
  }
}

/* --- apply a color scheme to all terminals --- */
static void apply_color_scheme (int idx) {
  if (idx < 0 || idx >= NUM_SCHEMES) return;
  const color_scheme_t &s = color_schemes[idx];
  for (rxvt_term *t : rxvt_term::termlist) {
    t->set_window_color (Color_fg, s.palette[0]);
    t->set_window_color (Color_bg, s.palette[1]);
    for (int i = 0; i < 16; i++)
      t->set_window_color (minCOLOR + i, s.palette[2 + i]);
  }
  s_active_scheme = idx;
}

/* --- apply changed settings to all terminals --- */
static void apply_settings (int changed) {
  if (changed & CHANGED_COLOR_SCHEME) {
    apply_color_scheme (s_pending_scheme);
    s_pending_scheme = -1;
  }

  if (changed & CHANGED_FONT) {
    int idx = s_pending_font;
    if (idx >= 0 && idx < NUM_FONTS) {
      for (rxvt_term *t : rxvt_term::termlist) {
        free ((void *) t->rs[Rs_font]);
        t->rs[Rs_font] = strdup (font_entries[idx].spec);
        t->set_fonts ();
      }
      s_active_font = idx;
    }
    s_pending_font = -1;
  }

  for (rxvt_term *t : rxvt_term::termlist) {
    if (changed & CHANGED_BORDER) {
      t->int_bwidth = (int) s_border_width;
      t->resize_all_windows (t->szHint.width, t->szHint.height, 1);
    }
    if (changed & CHANGED_SCROLL)
      t->wheel_scroll_lines = (int) s_scroll_speed;
    if (changed & CHANGED_CURSOR_BLINK) {
      t->set_option (Opt_cursorBlink, s_cursor_blink);
#ifdef CURSOR_BLINK
      t->cursor_blink_reset ();
#endif
      t->want_refresh = 1;
    }
    if (changed & CHANGED_CURSOR_UL) {
      t->set_option (Opt_cursorUnderline, s_cursor_underline);
      t->want_refresh = 1;
    }
    if (changed & CHANGED_SHADING) {
      t->bg_shading = (int) s_shading;
#ifdef HAVE_BG_PIXMAP
      t->bg_init ();
      t->bg_render ();
#endif
    }
    if (changed & CHANGED_LINE_SPACE) {
      t->lineSpace = (int) s_line_space;
      t->set_fonts ();
    }
    if (changed & CHANGED_LETTER_SPACE) {
      t->letterSpace = (int) s_letter_space;
      t->set_fonts ();
    }
    if (changed & CHANGED_SAVE_LINES) {
      t->saveLines = (int) s_save_lines;
      t->scr_reset ();
    }
    if (changed & CHANGED_SCROLLBAR) {
      t->set_option (Opt_scrollBar, s_scrollbar);
      t->scrollBar.state = s_scrollbar ? SB_STATE_IDLE : SB_STATE_OFF;
      t->resize_all_windows (t->szHint.width, t->szHint.height, 0);
    }
    if (changed & CHANGED_SCROLL_OUTPUT)
      t->set_option (Opt_scrollTtyOutput, s_scroll_on_output);
    if (changed & CHANGED_SCROLL_KEY)
      t->set_option (Opt_scrollTtyKeypress, s_scroll_on_keypress);
    if (changed & CHANGED_JUMP_SCROLL)
      t->set_option (Opt_jumpScroll, s_jump_scroll);
    if (changed & CHANGED_VISUAL_BELL)
      t->set_option (Opt_visualBell, s_visual_bell);
    if (changed & CHANGED_URGENT_BELL)
      t->set_option (Opt_urgentOnBell, s_urgent_on_bell);
    if (changed & CHANGED_WHEEL_PAGE)
      t->set_option (Opt_mouseWheelScrollPage, s_mouse_wheel_page);
#ifdef POINTER_BLANK
    if (changed & CHANGED_PTR_BLANK)
      t->set_option (Opt_pointerBlank, s_pointer_blank);
#endif
  }
}

/* --- restore snapshot and revert all settings --- */
static void restore_snapshot () {
  s_border_width       = s_snapshot.border_width;
  s_scroll_speed       = s_snapshot.scroll_speed;
  s_shading            = s_snapshot.shading;
  s_line_space         = s_snapshot.line_space;
  s_letter_space       = s_snapshot.letter_space;
  s_save_lines         = s_snapshot.save_lines;
  s_cursor_blink       = s_snapshot.cursor_blink;
  s_cursor_underline   = s_snapshot.cursor_underline;
  s_scrollbar          = s_snapshot.scrollbar;
  s_scroll_on_output   = s_snapshot.scroll_on_output;
  s_scroll_on_keypress = s_snapshot.scroll_on_keypress;
  s_jump_scroll        = s_snapshot.jump_scroll;
  s_visual_bell        = s_snapshot.visual_bell;
  s_urgent_on_bell     = s_snapshot.urgent_on_bell;
  s_mouse_wheel_page   = s_snapshot.mouse_wheel_page;
  s_pointer_blank      = s_snapshot.pointer_blank;
  s_active_scheme      = s_snapshot.active_scheme;
  s_active_font        = s_snapshot.active_font;

  int all = CHANGED_BORDER | CHANGED_SCROLL | CHANGED_CURSOR_BLINK |
            CHANGED_SHADING | CHANGED_LINE_SPACE | CHANGED_LETTER_SPACE |
            CHANGED_SAVE_LINES | CHANGED_CURSOR_UL | CHANGED_SCROLLBAR |
            CHANGED_SCROLL_OUTPUT | CHANGED_SCROLL_KEY | CHANGED_JUMP_SCROLL |
            CHANGED_VISUAL_BELL | CHANGED_URGENT_BELL | CHANGED_WHEEL_PAGE |
            CHANGED_PTR_BLANK;
  apply_settings (all);

  for (rxvt_term *t : rxvt_term::termlist) {
    t->set_window_color (Color_fg, s_snapshot.colors[0]);
    t->set_window_color (Color_bg, s_snapshot.colors[1]);
    for (int i = 0; i < 16; i++)
      t->set_window_color (minCOLOR + i, s_snapshot.colors[2 + i]);
    if (s_snapshot.font) {
      free ((void *) t->rs[Rs_font]);
      t->rs[Rs_font] = strdup (s_snapshot.font);
      t->set_fonts ();
    }
  }
}

/* --- read initial values from the terminal --- */
static void read_settings_from_term (rxvt_term *t) {
  s_border_width       = (float) t->int_bwidth;
  s_scroll_speed       = (float) t->wheel_scroll_lines;
  s_shading            = (float) t->bg_shading;
  s_line_space         = (float) t->lineSpace;
  s_letter_space       = (float) t->letterSpace;
  s_save_lines         = (float) t->saveLines;
  s_cursor_blink       = t->option (Opt_cursorBlink)          ? 1 : 0;
  s_cursor_underline   = t->option (Opt_cursorUnderline)      ? 1 : 0;
  s_scrollbar          = t->option (Opt_scrollBar)            ? 1 : 0;
  s_scroll_on_output   = t->option (Opt_scrollTtyOutput)      ? 1 : 0;
  s_scroll_on_keypress = t->option (Opt_scrollTtyKeypress)    ? 1 : 0;
  s_jump_scroll        = t->option (Opt_jumpScroll)           ? 1 : 0;
  s_visual_bell        = t->option (Opt_visualBell)           ? 1 : 0;
  s_urgent_on_bell     = t->option (Opt_urgentOnBell)         ? 1 : 0;
  s_mouse_wheel_page   = t->option (Opt_mouseWheelScrollPage) ? 1 : 0;
#ifdef POINTER_BLANK
  s_pointer_blank      = t->option (Opt_pointerBlank)         ? 1 : 0;
#endif
}

/* ======================================================================= */

void
rxvt_term::init_settings_ui ()
{
  settings_ui.visible      = false;
  settings_ui.win          = None;
  settings_ui.backdrop_win = None;
  settings_ui.backdrop_buf = None;
  settings_ui.gc           = None;
  settings_ui.width        = PANEL_WIDTH;
  settings_ui.height       = 0;
  settings_ui.parent_w     = 0;
  settings_ui.parent_h     = 0;
}

/* Refresh the full-size backdrop: copy vt content to the backdrop window
   with a dark overlay.  backdrop_win covers the entire terminal at (0,0) and
   shows the live (dimmed) terminal content behind the settings panel.
   In split mode both panes' vt windows are captured. */
static void backdrop_refresh (rxvt_term *t)
{
  if (t->settings_ui.backdrop_win == None ||
      t->settings_ui.backdrop_buf == None) return;

  Display *dpy = t->dpy;
  GC       gc  = t->settings_ui.gc;
  Pixmap   pix = t->settings_ui.backdrop_buf;
  int      pw  = t->settings_ui.parent_w;
  int      ph  = t->settings_ui.parent_h;

  /* Capture from the currently active terminal (GET_R), not the root term,
     since root's vt may be unmapped when on another tab. */
  rxvt_term *active = GET_R;

  /* Get the position of active's parent within the root window.
     For root term, parent is the root window at (0,0).
     For split child, parent is a child of root window at (attr.x, attr.y). */
  XWindowAttributes parent_attr;
  XGetWindowAttributes (dpy, active->parent, &parent_attr);

  /* Primary pane (active term's vt): copy at its position within root_win. */
  XCopyArea (dpy, active->vt, pix, gc,
             0, 0, active->vt_width, active->vt_height,
             parent_attr.x + active->window_vt_x,
             parent_attr.y + active->window_vt_y);

  /* In split mode also capture the other pane's vt. */
  if (active->split_partner) {
    rxvt_term *other = active->split_is_child ? active->split_partner : active->split_partner;
    if (other == active) other = nullptr; /* sanity check */

    if (other) {
      XWindowAttributes other_parent_attr;
      XGetWindowAttributes (dpy, other->parent, &other_parent_attr);
      XCopyArea (dpy, other->vt, pix, gc,
                 0, 0, other->vt_width, other->vt_height,
                 other_parent_attr.x + other->window_vt_x,
                 other_parent_attr.y + other->window_vt_y);
    }
  }

  XRenderPictFormat *fmt = XRenderFindVisualFormat (dpy, t->visual);
  if (fmt) {
    Picture pic = XRenderCreatePicture (dpy, pix, fmt, 0, nullptr);
    XRenderColor dark = {0, 0, 0, 0x4000}; /* ~56% opacity */
    XRenderFillRectangle (dpy, PictOpOver, pic, &dark, 0, 0, pw, ph);
    XRenderFreePicture (dpy, pic);
  }

  XCopyArea (dpy, pix, t->settings_ui.backdrop_win, gc, 0, 0, pw, ph, 0, 0);
}

void
rxvt_term::show_settings_ui ()
{
  /* Always operate on the root term so state, windows, and event watchers
     live on the longest-lived instance regardless of which pane triggered us. */
  if (this != termlist.at (0)) { termlist.at (0)->show_settings_ui (); return; }

  if (settings_ui.visible) return;

  rxvt_term *root_term = termlist.at (0);
  Window     root_win  = root_term->parent;

  int pw, ph;
  {
    XWindowAttributes wattr;
    XGetWindowAttributes (dpy, root_win, &wattr);
    pw = wattr.width  ? wattr.width  : 800;
    ph = wattr.height ? wattr.height : 600;
  }

  int panel_w = (PANEL_WIDTH < pw) ? PANEL_WIDTH : pw;
  int panel_h = ph;
  int panel_x = pw - panel_w;

  s_panel_h = panel_h;

  int scr = DefaultScreen (dpy);

  if (settings_ui.win == None) {
    /* First time: create backdrop (full size, behind everything) and panel. */
    XSetWindowAttributes attr = {};
    attr.background_pixel = BlackPixel (dpy, scr);
    attr.border_pixel     = 0;
    attr.colormap         = cmap;

    settings_ui.backdrop_win = XCreateWindow (
      dpy, root_win,
      0, 0, pw, ph, 0,
      depth, InputOutput, visual,
      CWBackPixel | CWBorderPixel | CWColormap, &attr);

    settings_ui.backdrop_buf = XCreatePixmap (dpy, root_win, pw, ph, depth);

    settings_ui.win = XCreateWindow (
      dpy, root_win,
      panel_x, 0, panel_w, panel_h, 0,
      depth, InputOutput, visual,
      CWBackPixel | CWBorderPixel | CWColormap, &attr);
    if (!settings_ui.win) return;

    settings_ui.gc = XCreateGC (dpy, root_win, 0, nullptr);
    if (!settings_ui.gc) {
      XDestroyWindow (dpy, settings_ui.win);
      settings_ui.win = None;
      return;
    }

    settings_ui_ev.start (display, settings_ui.win);

    if (!mu_ctx) {
      mu_ctx = (mu_Context *) malloc (sizeof (mu_Context));
      mu_init (mu_ctx);
      mu_ctx->text_width  = text_width_cb;
      mu_ctx->font_height = font_height_cb;
    }

    int res = r_init (dpy, settings_ui.win, settings_ui.gc, visual, depth, panel_w, panel_h);
    if (res != 0) {
      printf("could not init renderer\n");
      return;
    }

  } else {
    /* Reusing existing windows: update size/position. */
    XResizeWindow (dpy, settings_ui.backdrop_win, pw, ph);
    if (settings_ui.backdrop_buf != None)
      XFreePixmap (dpy, settings_ui.backdrop_buf);
      settings_ui.backdrop_buf = XCreatePixmap (dpy, root_win, pw, ph, depth);

    XMoveResizeWindow (dpy, settings_ui.win, panel_x, 0, panel_w, panel_h);
    r_resize (panel_w, panel_h);
  }

  settings_ui.width    = panel_w;
  settings_ui.height   = panel_h;
  settings_ui.parent_w = pw;
  settings_ui.parent_h = ph;

  read_settings_from_term (this);
  save_snapshot (this);

  settings_ui.visible = true;

  /* Enable Always backing store so XCopyArea in backdrop_refresh reads the
     actual repainted content even while the backdrop covers the terminal.
     backdrop_refresh reads from child->parent (not child->vt) so child->parent
     must also have backing store — X routes child->vt draws into child->parent's
     backing store when child->vt itself is not individually backed. */
  {
    XSetWindowAttributes bsa;
    bsa.backing_store = Always;
    for (rxvt_term *t : termlist) {
      XChangeWindowAttributes (dpy, t->vt, CWBackingStore, &bsa);
      if (t->split_partner && !t->split_is_child)
        XChangeWindowAttributes (dpy, t->split_partner->parent, CWBackingStore, &bsa);
    }
  }

  /* Capture terminal content into the backdrop buffer before the backdrop
     window is mapped.  This ensures a correct first frame with no black flash. */
  backdrop_refresh (this);

  /* Use XMapRaised for both so they land on top of any split-pane child
     windows that are already mapped as siblings of root_win. */
  XMapRaised (dpy, settings_ui.backdrop_win);
  XMapRaised (dpy, settings_ui.win);

  XSetInputFocus (dpy, settings_ui.win, RevertToParent, CurrentTime);
  XFlush (dpy);

  settings_ui_refresh_ev.set (0.0, 1.0 / 30.0);
  settings_ui_refresh_ev.start ();

  draw_settings_ui ();
}

void
rxvt_term::hide_settings_ui ()
{
  if (this != termlist.at (0)) { termlist.at (0)->hide_settings_ui (); return; }

  if (!settings_ui.visible) return;
  settings_ui.visible = false;
  settings_ui_refresh_ev.stop ();

  /* Return focus to whichever pane is currently active. */
  XSetInputFocus (dpy, GET_R->parent, RevertToPointerRoot, CurrentTime);

  XUnmapWindow (dpy, settings_ui.win);
  if (settings_ui.backdrop_win != None)
    XUnmapWindow (dpy, settings_ui.backdrop_win);

  /* Restore default backing store now that the backdrop is gone. */
  {
    XSetWindowAttributes bsa;
    bsa.backing_store = NotUseful;
    for (rxvt_term *t : termlist) {
      XChangeWindowAttributes (dpy, t->vt, CWBackingStore, &bsa);
      if (t->split_partner && !t->split_is_child)
        XChangeWindowAttributes (dpy, t->split_partner->parent, CWBackingStore, &bsa);
    }
  }

  XFlush (dpy);

  /* Repaint only the active pane now that the backdrop is gone.
     Touching non-active tabs can cause issues (e.g., stale selection state
     on split panes that weren't visible). */
  GET_R->scr_touch (true);
}

void
rxvt_term::recenter_settings_ui ()
{
  /* Must run on root term: only it owns the settings windows, and only it
     has the correct (full-window) dimensions — split panes have half-size
     szHint values after apply_split_geometry. */
  if (this != termlist.at (0)) { termlist.at (0)->recenter_settings_ui (); return; }

  if (!settings_ui.visible || settings_ui.win == None) return;

  /* Query the actual WM window size rather than szHint, which may have been
     corrupted to half-size by apply_split_geometry in split mode. */
  XWindowAttributes wattr;
  XGetWindowAttributes (dpy, parent, &wattr);
  int pw = wattr.width  ? wattr.width  : settings_ui.parent_w;
  int ph = wattr.height ? wattr.height : settings_ui.parent_h;
  if (!pw || !ph) return;
  if (pw == settings_ui.parent_w && ph == settings_ui.parent_h) return;

  int panel_w = (PANEL_WIDTH < pw) ? PANEL_WIDTH : pw;
  int panel_h = ph;
  int panel_x = pw - panel_w;

  s_panel_h            = panel_h;
  settings_ui.width    = panel_w;
  settings_ui.height   = panel_h;
  settings_ui.parent_w = pw;
  settings_ui.parent_h = ph;

  rxvt_term *root_term = termlist.at (0);
  Window     root_win  = root_term->parent;

  /* Backdrop covers the full terminal area. */
  if (settings_ui.backdrop_win != None)
    XResizeWindow (dpy, settings_ui.backdrop_win, pw, ph);

  if (settings_ui.backdrop_buf != None)
    XFreePixmap (dpy, settings_ui.backdrop_buf);
  settings_ui.backdrop_buf = XCreatePixmap (dpy, root_win, pw, ph, depth);

  /* Panel moves to the new right edge. */
  XMoveResizeWindow (dpy, settings_ui.win, panel_x, 0, panel_w, panel_h);
  r_resize (panel_w, panel_h);

  /* Re-raise both windows in case split operations stacked something above them. */
  XRaiseWindow (dpy, settings_ui.backdrop_win);
  XRaiseWindow (dpy, settings_ui.win);
  XFlush (dpy);

  draw_settings_ui ();
}

void
rxvt_term::destroy_settings_ui ()
{
  settings_ui_refresh_ev.stop ();
  if (settings_ui.win != None) {
    if (display) settings_ui_ev.stop (display);
    if (settings_ui.gc != None) { XFreeGC (dpy, settings_ui.gc); settings_ui.gc = None; }
    if (settings_ui.backdrop_buf != None) {
      XFreePixmap (dpy, settings_ui.backdrop_buf);
      settings_ui.backdrop_buf = None;
    }
    if (settings_ui.backdrop_win != None) {
      XDestroyWindow (dpy, settings_ui.backdrop_win);
      settings_ui.backdrop_win = None;
    }
    XDestroyWindow (dpy, settings_ui.win);
    settings_ui.win = None;

    if (mu_ctx) { free (mu_ctx); mu_ctx = nullptr; }
  }
  settings_ui.visible = false;
}

void
rxvt_term::draw_settings_ui ()
{
  if (!settings_ui.visible || !mu_ctx) return;

  mu_begin (mu_ctx);
  int changed = build_settings_window (mu_ctx);
  mu_end (mu_ctx);

  r_clear (mu_color (0x34, 0x35, 0x38, 255));
  render_mu (mu_ctx);
  r_present ();

  if (changed & CHANGED_CANCEL) {
    restore_snapshot ();

    for (rxvt_term *t : rxvt_term::termlist)
      t->refresh_check ();

    hide_settings_ui ();
    return;
  }

  if (changed & CHANGED_APPLY) {
    hide_settings_ui ();
    return;
  }

  if (changed) {
    apply_settings (changed);
    /* Start the flush timer for every pane so colour/cursor changes are
       painted immediately.  Normally refresh_check() is only called for
       GET_R inside x_cb, so split child panes would never repaint from
       a programmatic settings change without this explicit kick. */
    for (rxvt_term *t : rxvt_term::termlist)
      t->refresh_check ();
    /* Re-assert focus: some operations (set_fonts → resize_all_windows)
       can cause the X server or WM to redirect focus away from our panel. */
    if (settings_ui.visible)
      XSetInputFocus (dpy, settings_ui.win, RevertToParent, CurrentTime);
  }
}

void
rxvt_term::x_settings_ui_cb (XEvent &xev)
{
  if (!mu_ctx)  {
    return;
  }

  switch (xev.type) {
    case Expose:
      // if (xev.xexpose.count == 0)
      //   draw_settings_ui ();

      break;

    case MotionNotify:
      s_mousex = xev.xmotion.x;
      s_mousey = xev.xmotion.y;
      mu_input_mousemove (mu_ctx, s_mousex, s_mousey);
      // draw_settings_ui ();
      break;

    case ButtonPress:
      s_mousex = xev.xbutton.x;
      s_mousey = xev.xbutton.y;

      if (xev.xbutton.button == Button1)
        mu_input_mousedown (mu_ctx, s_mousex, s_mousey, MU_MOUSE_LEFT);
      else if (xev.xbutton.button == Button3)
        mu_input_mousedown (mu_ctx, s_mousex, s_mousey, MU_MOUSE_RIGHT);
      else if (xev.xbutton.button == Button4)
        mu_input_scroll (mu_ctx, 0, -30);
      else if (xev.xbutton.button == Button5)
        mu_input_scroll (mu_ctx, 0,  30);

      draw_settings_ui ();
      break;

    case ButtonRelease:
      s_mousex = xev.xbutton.x;
      s_mousey = xev.xbutton.y;
      if (xev.xbutton.button == Button1)
        mu_input_mouseup (mu_ctx, s_mousex, s_mousey, MU_MOUSE_LEFT);
      else if (xev.xbutton.button == Button3)
        mu_input_mouseup (mu_ctx, s_mousex, s_mousey, MU_MOUSE_RIGHT);

      draw_settings_ui ();
      break;

    case KeyPress: {
      KeySym ks = XLookupKeysym (&xev.xkey, 0);
      if (ks == XK_Escape) { hide_settings_ui (); return; }
      if (ks == XK_Return)    mu_input_keydown (mu_ctx, MU_KEY_RETURN);
      if (ks == XK_BackSpace) mu_input_keydown (mu_ctx, MU_KEY_BACKSPACE);
      char buf[8];
      int len = XLookupString (&xev.xkey, buf, sizeof (buf) - 1, nullptr, nullptr);
      if (len > 0) { buf[len] = '\0'; mu_input_text (mu_ctx, buf); }
      draw_settings_ui ();
      break;
    }

    case KeyRelease: {
      KeySym ks = XLookupKeysym (&xev.xkey, 0);
      if (ks == XK_Return)    mu_input_keyup (mu_ctx, MU_KEY_RETURN);
      if (ks == XK_BackSpace) mu_input_keyup (mu_ctx, MU_KEY_BACKSPACE);
      break;
    }
  }
}

void
rxvt_term::settings_ui_refresh_cb (ev::timer &w, int revents)
{
  if (settings_ui.visible) {
    /* Draw the settings panel first — this may call apply_settings which
       repaints the vt windows (via scr_recolor → scr_refresh) with new colors
       and flushes X requests.  Then capture the backdrop so it reflects the
       changes made in this same tick rather than lagging one frame behind. */
    draw_settings_ui ();
    XFlush (dpy);
    backdrop_refresh (this);
  }
}
