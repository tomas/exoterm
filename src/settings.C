/*----------------------------------------------------------------------*
 * File:        settings.C - microui-based settings side panel
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

#include <glib.h>
#include <glib/gstdio.h>

#include <sys/stat.h>
#include <algorithm>

/* Panel sits flush on the right edge, full parent height. */
#define PANEL_WIDTH 320

/* Current panel height — set dynamically to the parent window height. */
static int s_panel_h = 600;

/* --- renderer window tracking (shared by settings and context menu) --- */
static bool   s_renderer_ok  = false;
static Window s_renderer_win = None;

static void s_switch_renderer (Window win, int w, int h) {
  if (win != s_renderer_win) {
    r_switch_window (win, w, h);
    s_renderer_win = win;
  }
}

/* --- microui context (settings panel) --- */
static mu_Context *mu_ctx = nullptr;
static int s_mousex = 0, s_mousey = 0;

/* --- context menu --- */
#define CM_W       220
#define CM_ITEM_H   30
#define CM_SEP_H     2   /* separator row height (thin line, no ABSOLUTE trick) */
#define CM_PAD       8   /* matches mu default style padding */
#define CM_SPC       6   /* matches mu default style spacing */

static mu_Context  *mu_ctx_menu   = nullptr;
static rxvt_term   *s_cm_invoker  = nullptr;

enum {
  CM_NONE = 0,
  CM_NEW_TAB,
  CM_NEW_WINDOW,
  CM_SPLIT_H,
  CM_SPLIT_V,
  CM_CLOSE_SPLIT,
  CM_COPY,
  CM_PASTE,
  CM_SHOW_SETTINGS,
  CM_TOGGLE_MINIMAP,
  CM_CLOSE_TAB,
};

/* Height of the context menu window based on state.
   Formula: 2*CM_PAD + n_items*CM_ITEM_H + n_seps*CM_SEP_H + (n_rows-1)*CM_SPC
   Items (no-split): New Tab, New Window, Copy, Paste, Split H, Split V,
                     Show Settings, Close Tab = 8
   Items (split):    same but (Split H + Split V) → Close Split = 7
   Each +minimap adds 1 item and 1 separator. */
static int cm_compute_h (bool has_split, bool has_minimap) {
  int n_items = has_split ? 7 : 8;
  if (has_minimap) n_items += 1;
  // int n_seps  = has_minimap ? 5 : 4;
  int n_seps  = 4;
  return CM_PAD
       + n_items * CM_ITEM_H
       + n_seps  * CM_SEP_H
       + (n_items + n_seps - 1) * CM_SPC;
}

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
static char *s_active_font_xlfd   = nullptr;
static int   s_login_shell        = 0;
static int   s_skip_builtin_glyphs = 0;
static int   s_transparent        = 0;
static int   s_auto_copy_sel      = 1;
static char  s_cursor_color[16]   = "";
static char  s_geometry[32]       = "";

/* ======================================================================
   Snapshot — saved on open, restored on Cancel
   ====================================================================== */
struct settings_snapshot_t {
  float border_width, scroll_speed, shading, line_space, letter_space;
  float save_lines;
  int   cursor_blink, cursor_underline, scrollbar;
  int   scroll_on_output, scroll_on_keypress, jump_scroll;
  int   visual_bell, urgent_on_bell, mouse_wheel_page, pointer_blank;
  char  colors[18][16]; /* "[alpha]#rrggbb\0" for fg, bg, ANSI 0-15 */
  char *font;          /* owned copy of rs[Rs_font], may be NULL */
  int   active_scheme, active_font;
  int   login_shell, skip_builtin_glyphs, transparent, auto_copy_sel;
  char  cursor_color[16];
  char  geometry[32];
};

static settings_snapshot_t s_snapshot = {};

static void color_to_hex (rxvt_term *t, int idx, char *dst) {
  rgba c = (rgba) t->pix_colors_focused[idx];
  if (c.a < rgba::MAX_CC) {
    int alpha_pct = (int)((uint32_t)c.a * 100 / rgba::MAX_CC);
    snprintf (dst, 16, "[%d]#%02x%02x%02x", alpha_pct, c.r >> 8, c.g >> 8, c.b >> 8);
  } else {
    snprintf (dst, 16, "#%02x%02x%02x", c.r >> 8, c.g >> 8, c.b >> 8);
  }
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
  s_snapshot.login_shell        = s_login_shell;
  s_snapshot.skip_builtin_glyphs = s_skip_builtin_glyphs;
  s_snapshot.transparent        = s_transparent;
  s_snapshot.auto_copy_sel      = s_auto_copy_sel;
  memcpy (s_snapshot.cursor_color, s_cursor_color, sizeof (s_cursor_color));
  memcpy (s_snapshot.geometry,     s_geometry,     sizeof (s_geometry));

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
  { "Ayu Dark", {
    "#cccac2", "#0d1017",
    "#11151c", "#ea6c73", "#91b362", "#f9af4f",
    "#53bdfa", "#fae994", "#90e1c6", "#c7c7c7",
    "#686868", "#f07178", "#c2d94c", "#ffb454",
    "#59c2ff", "#ffee99", "#95e6cb", "#ffffff",
  }},
  { "Catppuccin Mocha", {
    "#cdd6f4", "#1e1e2e",
    "#45475a", "#f38ba8", "#a6e3a1", "#f9e2af",
    "#89b4fa", "#f5c2e7", "#94e2d5", "#bac2de",
    "#585b70", "#f38ba8", "#a6e3a1", "#f9e2af",
    "#89b4fa", "#f5c2e7", "#94e2d5", "#a6adc8",
  }},
  { "Cobalt2", {
    "#ffffff", "#193549",
    "#000000", "#ff0000", "#00c200", "#c7c400",
    "#0225c7", "#ca30c7", "#00c5c7", "#c7c7c7",
    "#686868", "#ff6e67", "#5ffa68", "#fffc67",
    "#6871ff", "#ff77ff", "#60fdff", "#ffffff",
  }},
  { "Dracula", {
    "#f8f8f2", "#282a36",
    "#21222c", "#ff5555", "#50fa7b", "#f1fa8c",
    "#bd93f9", "#ff79c6", "#8be9fd", "#f8f8f2",
    "#6272a4", "#ff6e6e", "#69ff94", "#ffffa5",
    "#d6acff", "#ff92df", "#a4ffff", "#ffffff",
  }},
  { "Everforest Dark", {
    "#d3c6aa", "#2d353b",
    "#475258", "#e67e80", "#a7c080", "#dbbc7f",
    "#7fbbb3", "#d699b6", "#83c092", "#d3c6aa",
    "#475258", "#e67e80", "#a7c080", "#dbbc7f",
    "#7fbbb3", "#d699b6", "#83c092", "#d3c6aa",
  }},
  { "Gruvbox Dark", {
    "#ebdbb2", "#282828",
    "#282828", "#cc241d", "#98971a", "#d79921",
    "#458588", "#b16286", "#689d6a", "#a89984",
    "#928374", "#fb4934", "#b8bb26", "#fabd2f",
    "#83a598", "#d3869b", "#8ec07c", "#ebdbb2",
  }},
  { "Horizon", {
    "#d5d8da", "#1c1e26",
    "#16161c", "#e95678", "#29d398", "#fab795",
    "#26bbd9", "#ee64ae", "#59e3e3", "#d5d8da",
    "#595f6f", "#ec6a88", "#3fdaa4", "#fbc3a7",
    "#3fc6de", "#f075b7", "#6be6e6", "#d5d8da",
  }},
  { "Iceberg", {
    "#c6c8d1", "#161821",
    "#1e2132", "#e27878", "#b4be82", "#e2a478",
    "#84a0c6", "#a093c7", "#89b8c2", "#c6c8d1",
    "#6b7089", "#e98989", "#c0ca8e", "#e9b189",
    "#91acd1", "#ada0d3", "#95c4ce", "#d2d4de",
  }},
  { "Kanagawa", {
    "#dcd7ba", "#1f1f28",
    "#090618", "#c34043", "#76946a", "#c0a36e",
    "#7e9cd8", "#957fb8", "#6a9589", "#c8c093",
    "#727169", "#e82424", "#98bb6c", "#e6c384",
    "#7fb4ca", "#938aa9", "#7aa89f", "#dcd7ba",
  }},
  { "Monokai", {
    "#f8f8f2", "#272822",
    "#272822", "#f92672", "#a6e22e", "#f4bf75",
    "#66d9e8", "#ae81ff", "#a1efe4", "#f8f8f2",
    "#75715e", "#f92672", "#a6e22e", "#f4bf75",
    "#66d9e8", "#ae81ff", "#a1efe4", "#f9f8f5",
  }},
  { "Nord", {
    "#d8dee9", "#2e3440",
    "#3b4252", "#bf616a", "#a3be8c", "#ebcb8b",
    "#81a1c1", "#b48ead", "#88c0d0", "#e5e9f0",
    "#4c566a", "#bf616a", "#a3be8c", "#ebcb8b",
    "#81a1c1", "#b48ead", "#8fbcbb", "#eceff4",
  }},
  { "One Dark", {
    "#abb2bf", "#282c34",
    "#282c34", "#e06c75", "#98c379", "#e5c07b",
    "#61afef", "#c678dd", "#56b6c2", "#abb2bf",
    "#5c6370", "#e06c75", "#98c379", "#e5c07b",
    "#61afef", "#c678dd", "#56b6c2", "#ffffff",
  }},
  { "Palenight", {
    "#a6accd", "#292d3e",
    "#292d3e", "#f07178", "#c3e88d", "#ffcb6b",
    "#82aaff", "#c792ea", "#89ddff", "#d0d0d0",
    "#434758", "#ff8b92", "#ddffa7", "#ffe585",
    "#9cc4ff", "#e1acff", "#a3f7ff", "#ffffff",
  }},
  { "Rose Pine", {
    "#e0def4", "#191724",
    "#26233a", "#eb6f92", "#31748f", "#f6c177",
    "#9ccfd8", "#c4a7e7", "#ebbcba", "#e0def4",
    "#6e6a86", "#eb6f92", "#31748f", "#f6c177",
    "#9ccfd8", "#c4a7e7", "#ebbcba", "#fffaf3",
  }},
  { "Rose Pine Moon", {
    "#e0def4", "#232136",
    "#393552", "#eb6f92", "#3e8fb0", "#f6c177",
    "#9ccfd8", "#c4a7e7", "#ea9a97", "#e0def4",
    "#59546d", "#eb6f92", "#3e8fb0", "#f6c177",
    "#9ccfd8", "#c4a7e7", "#ea9a97", "#faf4ed",
  }},
  { "Solarized Dark", {
    "#839496", "#002b36",
    "#073642", "#dc322f", "#859900", "#b58900",
    "#268bd2", "#d33682", "#2aa198", "#eee8d5",
    "#002b36", "#cb4b16", "#586e75", "#657b83",
    "#839496", "#6c71c4", "#93a1a1", "#fdf6e3",
  }},
  { "Tokyo Night", {
    "#a9b1d6", "#1a1b26",
    "#32344a", "#f7768e", "#9ece6a", "#e0af68",
    "#7aa2f7", "#ad8ee6", "#449dab", "#787c99",
    "#444b6a", "#ff7a93", "#b9f27c", "#ff9e64",
    "#7da6ff", "#bb9af7", "#0db9d7", "#acb0d0",
  }},
  { "Zenburn", {
    "#dcdccc", "#3f3f3f",
    "#1e2320", "#705050", "#60b48a", "#dfaf8f",
    "#506070", "#dc8cc3", "#8cd0d3", "#dcdccc",
    "#709080", "#dca3a3", "#72d5a3", "#f0dfaf",
    "#94bff3", "#ec93d3", "#93e0e3", "#ffffff",
  }},
};

static const int NUM_SCHEMES = (int)(sizeof (color_schemes) / sizeof (color_schemes[0]));

/* ======================================================================
   Font list — loaded dynamically from ~/.local/share/exoterm/fonts
   ====================================================================== */
struct font_entry_t {
  char *name;
  char *xlfd;
};

static font_entry_t *s_font_entries = nullptr;
static int s_num_fonts = 0;

static int   s_active_bold_font   = -1;
static int   s_pending_bold_font  = -1;
static char *s_active_bold_xlfd   = nullptr;

static char *xlfd_to_name(const char *xlfd) {
  if (!xlfd || xlfd[0] != '-') return xlfd ? strdup(xlfd) : strdup("Unknown");

  char *copy = strdup(xlfd);
  const char *fields[14] = {nullptr};
  int n = 0;
  char *p = copy;
  while (n < 14 && *p) {
    if (*p == '-') { *p = '\0'; fields[n++] = p + 1; }
    p++;
  }

  const char *foundry = fields[1] && fields[1][0] ? fields[1] : "misc";
  const char *weight  = fields[2] && strcmp(fields[2], "medium") != 0 ? fields[2] : "";
  const char *size    = fields[6] ? fields[6] : "?";

  char buf[128];
  if (weight[0]) {
    snprintf(buf, sizeof(buf), "%s %s %spx", foundry, weight, size);
  } else {
    snprintf(buf, sizeof(buf), "%s %spx", foundry, size);
  }
  free(copy);
  return strdup(buf);
}

static char *bold_xlfd(const char *xlfd) {
  if (!xlfd || xlfd[0] != '-') return nullptr;

  char *copy = strdup(xlfd);
  char *fields[14] = {nullptr};
  int n = 0;
  char *p = copy;
  while (n < 14 && *p) {
    if (*p == '-') { *p = '\0'; fields[n++] = p + 1; }
    p++;
  }

  if (!fields[0] || !fields[1] || !fields[2]) {
    free(copy);
    return nullptr;
  }

  char *foundry = fields[1];
  char *weight  = fields[2];

  if (strcmp(weight, "medium") == 0) {
    weight[0] = 'b'; weight[1] = 'o'; weight[2] = 'l'; weight[3] = 'd'; weight[4] = '\0';
  } else {
    free(copy);
    return nullptr;
  }

  char buf[512];
  snprintf(buf, sizeof(buf), "-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s",
    fields[0], foundry, weight,
    fields[3] ? fields[3] : "*",
    fields[4] ? fields[4] : "*",
    fields[5] ? fields[5] : "*",
    fields[6] ? fields[6] : "*",
    fields[7] ? fields[7] : "*",
    fields[8] ? fields[8] : "*",
    fields[9] ? fields[9] : "*",
    fields[10] ? fields[10] : "*",
    fields[11] ? fields[11] : "*",
    fields[12] ? fields[12] : "*");

  free(copy);
  return strdup(buf);
}

static int find_bold_font_idx(const char *bold_xlfd) {
  for (int i = 0; i < s_num_fonts; i++) {
    if (strcmp(s_font_entries[i].xlfd, bold_xlfd) == 0) {
      return i;
    }
  }
  return -1;
}

static void scan_fonts_dir(const char *dir) {
  char *fdir_path = g_build_filename(dir, "fonts.dir", NULL);
  char *contents = NULL;
  gsize len = 0;

  if (!g_file_get_contents(fdir_path, &contents, &len, NULL)) {
    g_free(fdir_path);
    return;
  }

  char *line = contents;
  int line_num = 0;

  while (line && *line) {
    char *next = strchr(line, '\n');
    if (next) { *next = '\0'; next++; }

    if (line_num > 0 && *line) {
      char *space = strchr(line, ' ');
      if (space) {
        *space = '\0';
        char *xlfd = space + 1;
        while (*xlfd == ' ') xlfd++;
        if (*xlfd) {
          s_font_entries = (font_entry_t *)realloc(s_font_entries,
            (s_num_fonts + 1) * sizeof(font_entry_t));
          s_font_entries[s_num_fonts].name = xlfd_to_name(xlfd);
          s_font_entries[s_num_fonts].xlfd = strdup(xlfd);
          s_num_fonts++;
        }
      }
    }
    line = next;
    line_num++;
  }

  g_free(contents);
  g_free(fdir_path);
}

static void ensure_font_dir(Display *dpy, const char *dir) {
  (void)dpy;
  struct stat st;
  if (stat(dir, &st) != 0) {
    g_mkdir_with_parents(dir, 0755);
  }

  char *cmd = g_strdup_printf("mkfontdir '%s' 2>/dev/null", dir);
  system(cmd);
  g_free(cmd);

  cmd = g_strdup_printf("xset +fp '%s'; xset fp rehash 2>/dev/null", dir);
  system(cmd);
  g_free(cmd);
}

static void init_font_list(Display *dpy) {
  for (int i = 0; i < s_num_fonts; i++) {
    free(s_font_entries[i].name);
    free(s_font_entries[i].xlfd);
  }
  free(s_font_entries);
  s_font_entries = nullptr;
  s_num_fonts = 0;

  char *user_dir = g_build_filename(g_get_user_data_dir(), "exoterm", "fonts", NULL);
  ensure_font_dir(dpy, user_dir);
  scan_fonts_dir(user_dir);
  g_free(user_dir);

  if (s_num_fonts == 0) {
    s_font_entries = (font_entry_t *)calloc(1, sizeof(font_entry_t));
    s_font_entries[0].name = strdup("(no fonts — see ~/.local/share/exoterm/fonts)");
    s_font_entries[0].xlfd = strdup("fixed");
    s_num_fonts = 1;
  }

  if (s_active_font_xlfd) {
    for (int i = 0; i < s_num_fonts; i++) {
      if (strcmp(s_font_entries[i].xlfd, s_active_font_xlfd) == 0) {
        s_active_font = i;
        break;
      }
    }
  }

  if (s_active_bold_xlfd) {
    /* Explicit bold font set — find it in the list (don't auto-detect). */
    for (int i = 0; i < s_num_fonts; i++) {
      if (strcmp(s_font_entries[i].xlfd, s_active_bold_xlfd) == 0) {
        s_active_bold_font = i;
        break;
      }
    }
  } else if (s_active_font >= 0 && s_active_font < s_num_fonts) {
    /* No explicit bold — auto-detect from the regular font. */
    char *bold = bold_xlfd(s_font_entries[s_active_font].xlfd);
    if (bold) {
      int bold_idx = find_bold_font_idx(bold);
      if (bold_idx >= 0) {
        s_active_bold_font = bold_idx;
        free(s_active_bold_xlfd);
        s_active_bold_xlfd = strdup(bold);
      }
      free(bold);
    }
  }
}

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
  CHANGED_BOLD_FONT      = 1 << 19,
  CHANGED_LOGIN_SHELL         = 1 << 20,
  CHANGED_SKIP_BUILTIN_GLYPHS = 1 << 21,
  CHANGED_TRANSPARENT         = 1 << 22,
  CHANGED_AUTO_COPY_SEL       = 1 << 23,
  CHANGED_CURSOR_COLOR        = 1 << 24,
  CHANGED_GEOMETRY            = 1 << 25,
  CHANGED_SAVE           = 1 << 28,
  CHANGED_APPLY          = 1 << 29,
  CHANGED_CANCEL         = 1 << 30,
  CHANGED_CLOSE          = 1 << 31,
};

/* --- section header --- */

static void input_label(mu_Context *ctx, const char *text) {
  mu_label_ex(ctx, text, ctx->style->colors[MU_COLOR_TEXT], -ctx->style->padding, 0, 1, 0);
}


static void section_header (mu_Context *ctx, const char *label) {
  { int c[] = {-1}; mu_layout_row (ctx, 1, c, 6); }
  mu_label (ctx, "", 0);
  { int c[] = {-1}; mu_layout_row (ctx, 1, c, 20); }
  // mu_label (ctx, label, 0);
  int scale = 1;
  mu_label_ex(ctx, label, mu_color (150, 150, 150, 255), -ctx->style->padding, 0, scale, 0);
}

/* --- build settings panel, return CHANGED_* bitmask --- */
static int build_settings_window (mu_Context *ctx) {
  int changed = 0;
  char *font_display_name = nullptr;
  char *bold_display_name = nullptr;
  mu_Rect rect = mu_rect (0, 0, PANEL_WIDTH, s_panel_h);

  int opt = MU_OPT_NORESIZE | MU_OPT_NODRAG;

  if (!mu_begin_window_ex (ctx, "Settings", rect, opt)) {
    /* window was closed via the X button */
    return CHANGED_CLOSE;
  }
  if (true) {
    int lw = 120; /* label column width — tuned for PANEL_WIDTH 320 */

    // set container to match the window's height
    mu_Container *win = mu_get_current_container(ctx);
    win->rect.h = s_panel_h;


    /* ---- Terminal ---- */
    section_header (ctx, "General");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }

    input_label (ctx, "Login shell");
    if (mu_checkbox (ctx, NULL, &s_login_shell) & MU_RES_CHANGE)
      changed |= CHANGED_LOGIN_SHELL;

    input_label (ctx, "Geometry");
    if (mu_textbox (ctx, s_geometry, sizeof (s_geometry)) & MU_RES_CHANGE)
      changed |= CHANGED_GEOMETRY;

    input_label (ctx, "Inner margin");
    if (float_slider (ctx, &s_border_width, 0.0f, 30.0f, 1.0f, "%.0f px") & MU_RES_CHANGE)
      changed |= CHANGED_BORDER;

// #ifdef HAVE_BG_PIXMAP
//     mu_label (ctx, "Transparent:", 0);
//     if (mu_checkbox (ctx, "##transp", &s_transparent) & MU_RES_CHANGE)
//       changed |= CHANGED_TRANSPARENT;
// #endif

    // mu_label (ctx, "Shading:", 0);
    // if (float_slider (ctx, &s_shading, 0.0f, 100.0f, 1.0f, "%.0f %%") & MU_RES_CHANGE)
    //   changed |= CHANGED_SHADING;

    /* ---- Color Scheme ---- */
    input_label (ctx, "Color Scheme");
    { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }
    const char *scheme_label = (s_active_scheme >= 0 && s_active_scheme < NUM_SCHEMES)
      ? color_schemes[s_active_scheme].name : "Custom";

    if (mu_begin_combo_ex (ctx, "##schemes", scheme_label, NUM_SCHEMES * 32, 0)) {
      for (int i = 0; i < NUM_SCHEMES; i++) {
        const color_scheme_t &s = color_schemes[i];
        char label[64];
        snprintf (label, sizeof (label), "%s%s", s.name, (s_active_scheme == i) ? " (active)" : "", s.name);
        /* reset to full-width single column before each name button */
        { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }
        bool clicked = (bool) mu_button (ctx, label);
        /* compact swatch strip — must re-set layout after the button */
        int sw = (PANEL_WIDTH - 110) / 8;


        { int widths[8]; for (int j = 0; j < 8; j++) widths[j] = sw;
          mu_layout_row (ctx, 8, widths, 4); }

        for (int j = 0; j < 8; j++) {
          mu_Rect r = mu_layout_next (ctx);
          mu_draw_rect (ctx, r, hex_to_mu (s.palette[2 + j]));
        }
        if (clicked) {
          s_pending_scheme = i;
          changed |= CHANGED_COLOR_SCHEME;
          mu_close_popup (ctx, "##schemes");
        }
      }
      mu_end_combo (ctx);
    }
    /* swatch strip for the currently active scheme, shown below the combo */
    if (s_active_scheme >= 0 && s_active_scheme < NUM_SCHEMES) {
      int sw = (PANEL_WIDTH - 73) / 8;
      { int widths[8]; for (int j = 0; j < 8; j++) widths[j] = sw;
        mu_layout_row (ctx, 8, widths, 5); }
      for (int j = 0; j < 8; j++) {
        mu_Rect r = mu_layout_next (ctx);
        mu_draw_rect (ctx, r, hex_to_mu (color_schemes[s_active_scheme].palette[2 + j]));
      }
    }

    section_header (ctx, "Text");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }

    input_label (ctx, "Line spacing");
    if (float_slider (ctx, &s_line_space, -4.0f, 16.0f, 1.0f, "%.0f px") & MU_RES_CHANGE)
      changed |= CHANGED_LINE_SPACE;


#ifdef BUILTIN_GLYPHS
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    input_label (ctx, "Skip built-in glyphs");
    if (mu_checkbox (ctx, NULL, &s_skip_builtin_glyphs) & MU_RES_CHANGE)
      changed |= CHANGED_SKIP_BUILTIN_GLYPHS;
#endif

    /* ---- Fonts ---- */
    input_label (ctx, "Main Font");
    { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }

    font_display_name = (s_active_font == -1)
      ? xlfd_to_name (s_active_font_xlfd)  /* loaded font not in list, derive a display name */
      : nullptr;
    const char *current = (s_active_font != -1)
      ? s_font_entries[s_active_font].name
      : (font_display_name && *font_display_name ? font_display_name : "Choose one");
    if (mu_begin_combo_ex(ctx, "##fonts", current, s_num_fonts * 34, 0)) {
      { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }
      for (int i = 0; i < s_num_fonts; i++) {
        char label[128];
        snprintf (label, sizeof (label), "%s%s", (s_active_font == i) ? ">  " : "   ", s_font_entries[i].name);
        if (mu_button(ctx, label)) {
          s_pending_font = i;
          changed |= CHANGED_FONT;
          mu_close_popup(ctx, "##fonts");
        }
      }
      mu_end_combo(ctx);
    }

    input_label (ctx, "Bold Font");

    bold_display_name = (s_active_bold_font == -1 && s_active_bold_xlfd)
      ? xlfd_to_name (s_active_bold_xlfd)
      : nullptr;
    const char *bold_current = (s_active_bold_font != -1)
      ? s_font_entries[s_active_bold_font].name
      : (bold_display_name ? bold_display_name : "Auto-detect");
    if (mu_begin_combo_ex(ctx, "##boldfonts", bold_current, s_num_fonts * 34, 0)) {
      { int c[] = {-1}; mu_layout_row (ctx, 1, c, 0); }
      if (mu_button(ctx, "   Auto-detect (from main font)")) {
        s_pending_bold_font = -1;
        changed |= CHANGED_BOLD_FONT;
        mu_close_popup(ctx, "##boldfonts");
      }
      for (int i = 0; i < s_num_fonts; i++) {
        char label[128];
        snprintf (label, sizeof (label), "%s%s",
            (s_active_bold_font == i) ? ">  " : "   ", s_font_entries[i].name);
        if (mu_button(ctx, label)) {
          s_pending_bold_font = i;
          changed |= CHANGED_BOLD_FONT;
          mu_close_popup(ctx, "##boldfonts");
        }
      }
      mu_end_combo(ctx);
    }

    /* ---- Cursor ---- */
    section_header (ctx, "Cursor & selection");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }

    input_label (ctx, "Autocopy selection");
    if (mu_checkbox (ctx, NULL, &s_auto_copy_sel) & MU_RES_CHANGE)
      changed |= CHANGED_AUTO_COPY_SEL;
    input_label (ctx, "Cursor Color");
    if (mu_textbox (ctx, s_cursor_color, sizeof (s_cursor_color)) & MU_RES_CHANGE)
      changed |= CHANGED_CURSOR_COLOR;
    input_label (ctx, "Cursor blink");
    if (mu_checkbox (ctx, NULL, &s_cursor_blink) & MU_RES_CHANGE)
      changed |= CHANGED_CURSOR_BLINK;
    input_label (ctx, "Underline");
    if (mu_checkbox (ctx, NULL, &s_cursor_underline) & MU_RES_CHANGE)
      changed |= CHANGED_CURSOR_UL;

    /* ---- Scrolling ---- */
    section_header (ctx, "Scrolling");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    input_label (ctx, "Speed");
    if (float_slider (ctx, &s_scroll_speed, 1.0f, 20.0f, 1.0f, "%.0f lines") & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL;
    input_label (ctx, "Scrollback");
    if (float_slider (ctx, &s_save_lines, 100.0f, 50000.0f, 100.0f, "%.0f") & MU_RES_CHANGE)
      changed |= CHANGED_SAVE_LINES;

    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    input_label (ctx, "Scrollbar");
    if (mu_checkbox (ctx, NULL, &s_scrollbar) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLLBAR;
    input_label (ctx, "Jump scroll");
    if (mu_checkbox (ctx, NULL, &s_jump_scroll) & MU_RES_CHANGE)
      changed |= CHANGED_JUMP_SCROLL;
    input_label (ctx, "On output");
    if (mu_checkbox (ctx, NULL, &s_scroll_on_output) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL_OUTPUT;
    input_label (ctx, "On keypress");
    if (mu_checkbox (ctx, NULL, &s_scroll_on_keypress) & MU_RES_CHANGE)
      changed |= CHANGED_SCROLL_KEY;
    input_label (ctx, "Page on wheel");
    if (mu_checkbox (ctx, NULL, &s_mouse_wheel_page) & MU_RES_CHANGE)
      changed |= CHANGED_WHEEL_PAGE;

    /* ---- Alerts & misc ---- */
    section_header (ctx, "Alerts & Misc");
    { int c[] = {lw, -1}; mu_layout_row (ctx, 2, c, 0); }
    input_label (ctx, "Visual bell");
    if (mu_checkbox (ctx, NULL, &s_visual_bell) & MU_RES_CHANGE)
      changed |= CHANGED_VISUAL_BELL;
    input_label (ctx, "Urgent on bell");
    if (mu_checkbox (ctx, NULL, &s_urgent_on_bell) & MU_RES_CHANGE)
      changed |= CHANGED_URGENT_BELL;
    input_label (ctx, "Blank pointer");
    if (mu_checkbox (ctx, NULL, &s_pointer_blank) & MU_RES_CHANGE)
      changed |= CHANGED_PTR_BLANK;


    /* ---- Bottom spacer (reserves room so content scrolls above buttons) ---- */
    { int c[] = {-1}; mu_layout_row (ctx, 1, c, 32); }
    mu_label (ctx, "", 0);

    /* ---- Buttons — pinned to bottom via absolute positioning ---- */
    {
      int btn_h = ctx->style->size.y + ctx->style->padding * 2;
      int btn_w = 60;
      int spc   = ctx->style->spacing;
      int pad   = ctx->style->padding;
      int y     = s_panel_h - btn_h - pad;
      int x3    = PANEL_WIDTH - (pad * 2.5) - btn_w;
      int x2    = x3 - spc - btn_w;
      int x1    = x2 - spc - btn_w;
      mu_layout_set_next (ctx, mu_rect (x1, y, btn_w, btn_h), 0);
      if (mu_button (ctx, "Cancel")) changed |= CHANGED_CANCEL;
      mu_layout_set_next (ctx, mu_rect (x2, y, btn_w, btn_h), 0);
      if (mu_button (ctx, "Apply"))  changed |= CHANGED_APPLY;
      mu_layout_set_next (ctx, mu_rect (x3, y, btn_w, btn_h), 0);
      if (mu_button (ctx, "Save"))   changed |= CHANGED_SAVE;
    }

    mu_end_window (ctx);
  }

  free (font_display_name);
  free (bold_display_name);
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
    /* Preserve the alpha channel of the existing colors.  Color scheme
       palettes define RGB only; any ARGB transparency the user configured
       (e.g. "[75]#282a36") lives in the current color's alpha and must be
       kept so that real (non-pseudo) transparency survives a scheme switch. */
    auto set_preserving_alpha = [&](int cidx, const char *hex) {
      rgba cur = (rgba) t->pix_colors_focused[cidx];
      if (cur.a < rgba::MAX_CC) {
        char buf[16];
        int alpha_pct = (int)((uint32_t)cur.a * 100 / rgba::MAX_CC);
        snprintf (buf, sizeof (buf), "[%d]%s", alpha_pct, hex);
        t->set_window_color (cidx, buf);
      } else {
        t->set_window_color (cidx, hex);
      }
    };
    set_preserving_alpha (Color_fg, s.palette[0]);
    set_preserving_alpha (Color_bg, s.palette[1]);
    set_preserving_alpha (Color_border, s.palette[1]); /* border tracks bg */
    for (int i = 0; i < 16; i++)
      set_preserving_alpha (minCOLOR + i, s.palette[2 + i]);
  }
  s_active_scheme = idx;
}

/* Replace t->rs[idx] safely, keeping t->allocated consistent to avoid
   double-free in ~rxvt_term.  The original string may or may not be
   tracked in allocated; either way we free it exactly once and track
   the replacement. */
static void replace_rs_str (rxvt_term *t, int idx, const char *newval)
{
  char *new_str = newval ? strdup (newval) : nullptr;
  const char *old = t->rs[idx];
  if (old) {
    auto &alloc = t->allocated;
    auto it = std::find (alloc.begin (), alloc.end (), (void *)old);
    if (it != alloc.end ()) {
      free (*it);
      *it = new_str;          /* replace in-place so slot stays valid */
    } else {
      free ((void *)old);
      if (new_str) alloc.push_back (new_str);
    }
  } else if (new_str) {
    t->allocated.push_back (new_str);
  }
  t->rs[idx] = new_str;
}

/* --- apply changed settings to all terminals --- */
static void apply_settings (int changed) {
  if (changed & CHANGED_COLOR_SCHEME) {
    apply_color_scheme (s_pending_scheme);
    s_pending_scheme = -1;
  }

  if (changed & CHANGED_FONT) {
    int idx = s_pending_font;
    if (idx >= 0 && idx < s_num_fonts) {
      for (rxvt_term *t : rxvt_term::termlist) {
        Window root;
        unsigned int w, h, bw, d;
        XGetGeometry(t->dpy, t->parent, &root, &t->window_vt_x, &t->window_vt_y, &w, &h, &bw, &d);

        replace_rs_str (t, Rs_font, s_font_entries[idx].xlfd);
        t->set_fonts ();

        XResizeWindow(t->dpy, t->parent, w, h);
      }
      s_active_font = idx;
      free(s_active_font_xlfd);
      s_active_font_xlfd = strdup(s_font_entries[idx].xlfd);

      char *bold = bold_xlfd(s_font_entries[idx].xlfd);
      if (bold) {
        int bold_idx = find_bold_font_idx(bold);
        if (bold_idx >= 0) {
          s_active_bold_font = bold_idx;
          free(s_active_bold_xlfd);
          s_active_bold_xlfd = strdup(bold);
          for (rxvt_term *t : rxvt_term::termlist) {
            replace_rs_str (t, Rs_boldFont, bold);
            t->set_fonts();
          }
        }
        free(bold);
      }
    }
    s_pending_font = -1;
  }

  if (changed & CHANGED_BOLD_FONT) {
    int idx = s_pending_bold_font;
    for (rxvt_term *t : rxvt_term::termlist) {
      Window root;
      unsigned int w, h, bw, d;
      XGetGeometry(t->dpy, t->parent, &root, &t->window_vt_x, &t->window_vt_y, &w, &h, &bw, &d);

      if (idx >= 0 && idx < s_num_fonts) {
        replace_rs_str (t, Rs_boldFont, s_font_entries[idx].xlfd);
        t->set_fonts ();
      } else {
        replace_rs_str (t, Rs_boldFont, nullptr);
        t->set_fonts ();
      }
      XResizeWindow(t->dpy, t->parent, w, h);
    }
    if (idx >= 0 && idx < s_num_fonts) {
      s_active_bold_font = idx;
      free(s_active_bold_xlfd);
      s_active_bold_xlfd = strdup(s_font_entries[idx].xlfd);
    } else {
      s_active_bold_font = -1;
      free(s_active_bold_xlfd);
      s_active_bold_xlfd = nullptr;
    }
    s_pending_bold_font = -1;
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
    if (changed & CHANGED_LOGIN_SHELL)
      t->set_option (Opt_loginShell, s_login_shell);
    if (changed & CHANGED_AUTO_COPY_SEL)
      t->set_option (Opt_autoCopySelection, s_auto_copy_sel);
#ifdef BUILTIN_GLYPHS
    if (changed & CHANGED_SKIP_BUILTIN_GLYPHS) {
      t->set_option (Opt_skipBuiltinGlyphs, s_skip_builtin_glyphs);
      t->set_fonts ();
      t->scr_remap_chars ();
    }
#endif
#ifdef HAVE_BG_PIXMAP
    if (changed & CHANGED_TRANSPARENT) {
      t->set_option (Opt_transparent, s_transparent);
      t->bg_init ();
      t->bg_render ();
    }
#endif
    if (changed & CHANGED_CURSOR_COLOR) {
      if (s_cursor_color[0])
        t->set_window_color (Color_cursor, s_cursor_color);
    }
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
  s_login_shell        = s_snapshot.login_shell;
  s_skip_builtin_glyphs = s_snapshot.skip_builtin_glyphs;
  s_transparent        = s_snapshot.transparent;
  s_auto_copy_sel      = s_snapshot.auto_copy_sel;
  memcpy (s_cursor_color, s_snapshot.cursor_color, sizeof (s_cursor_color));
  memcpy (s_geometry,     s_snapshot.geometry,     sizeof (s_geometry));

  free(s_active_font_xlfd);
  s_active_font_xlfd = s_snapshot.font ? strdup(s_snapshot.font) : nullptr;
  s_active_font = -1;
  free(s_active_bold_xlfd);
  s_active_bold_xlfd = nullptr;
  s_active_bold_font = -1;

  int all = CHANGED_BORDER | CHANGED_SCROLL | CHANGED_CURSOR_BLINK |
            CHANGED_SHADING | CHANGED_LINE_SPACE | CHANGED_LETTER_SPACE |
            CHANGED_SAVE_LINES | CHANGED_CURSOR_UL | CHANGED_SCROLLBAR |
            CHANGED_SCROLL_OUTPUT | CHANGED_SCROLL_KEY | CHANGED_JUMP_SCROLL |
            CHANGED_VISUAL_BELL | CHANGED_URGENT_BELL | CHANGED_WHEEL_PAGE |
            CHANGED_PTR_BLANK | CHANGED_LOGIN_SHELL | CHANGED_AUTO_COPY_SEL |
            CHANGED_SKIP_BUILTIN_GLYPHS | CHANGED_TRANSPARENT | CHANGED_CURSOR_COLOR;
  apply_settings (all);

  for (rxvt_term *t : rxvt_term::termlist) {
    t->set_window_color (Color_fg, s_snapshot.colors[0]);
    t->set_window_color (Color_bg, s_snapshot.colors[1]);
    for (int i = 0; i < 16; i++)
      t->set_window_color (minCOLOR + i, s_snapshot.colors[2 + i]);
    if (s_snapshot.font) {
      replace_rs_str (t, Rs_font, s_snapshot.font);
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
  s_login_shell        = t->option (Opt_loginShell)           ? 1 : 0;
  s_auto_copy_sel      = t->option (Opt_autoCopySelection)    ? 1 : 0;
#ifdef BUILTIN_GLYPHS
  s_skip_builtin_glyphs = t->option (Opt_skipBuiltinGlyphs)  ? 1 : 0;
#endif
#ifdef HAVE_BG_PIXMAP
  s_transparent        = t->option (Opt_transparent)          ? 1 : 0;
#endif

  /* cursor color */
  {
    char cc[16] = "";
#ifndef NO_CURSORCOLOR
    if (t->rs[Rs_color + Color_cursor])
      snprintf (cc, sizeof (cc), "%s", t->rs[Rs_color + Color_cursor]);
    else
      color_to_hex (t, Color_cursor, cc);
#endif
    memcpy (s_cursor_color, cc, sizeof (s_cursor_color));
  }

  /* geometry: prefer stored rs[Rs_geometry], else derive from current size */
  if (t->rs[Rs_geometry] && t->rs[Rs_geometry][0])
    snprintf (s_geometry, sizeof (s_geometry), "%s", t->rs[Rs_geometry]);
  else
    snprintf (s_geometry, sizeof (s_geometry), "%dx%d", t->ncol, t->nrow);

  free(s_active_font_xlfd);
  s_active_font_xlfd = t->rs[Rs_font] ? strdup(t->rs[Rs_font]) : nullptr;

  free(s_active_bold_xlfd);
  s_active_bold_xlfd = t->rs[Rs_boldFont] ? strdup(t->rs[Rs_boldFont]) : nullptr;

  /* Detect which (if any) named scheme matches the terminal's current colors. */
  auto rgb_of = [&](int cidx) -> const char * {
    /* color_to_hex produces "#rrggbb" or "[n]#rrggbb" — we only need the "#..." part */
    rgba c = (rgba) t->pix_colors_focused[cidx];
    static char buf[8];
    snprintf (buf, sizeof (buf), "#%02x%02x%02x", c.r >> 8, c.g >> 8, c.b >> 8);
    return buf;
  };
  s_active_scheme = -1;
  for (int i = 0; i < NUM_SCHEMES; i++) {
    const color_scheme_t &s = color_schemes[i];
    if (strcasecmp (rgb_of (Color_fg), s.palette[0]) != 0) continue;
    if (strcasecmp (rgb_of (Color_bg), s.palette[1]) != 0) continue;
    bool match = true;
    for (int j = 0; j < 16 && match; j++)
      if (strcasecmp (rgb_of (minCOLOR + j), s.palette[2 + j]) != 0)
        match = false;
    if (match) { s_active_scheme = i; break; }
  }
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

  /* Seed the entire pixmap with the border/background color so that the
     internal-border strips around the vt (internalBorder padding) are not
     left as undefined pixmap content.  XCopyArea from the vt only fills
     the vt area; the surrounding strips would otherwise show as stale or
     garbage pixels after the dark overlay is composited. */
  XSetForeground (dpy, gc, t->lookup_color (Color_border, t->pix_colors));
  XFillRectangle (dpy, pix, gc, 0, 0, pw, ph);

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

    if (!s_renderer_ok) {
      int res = r_init (dpy, settings_ui.win, settings_ui.gc, visual, depth, panel_w, panel_h);
      if (res != 0) {
        printf("could not init renderer\n");
        settings_ui_ev.stop (display);
        XDestroyWindow (dpy, settings_ui.win);
        settings_ui.win = None;
        return;
      }
      s_renderer_ok  = true;
      s_renderer_win = settings_ui.win;
    } else {
      s_switch_renderer (settings_ui.win, panel_w, panel_h);
    }

  } else {
    /* Reusing existing windows: update size/position. */
    XResizeWindow (dpy, settings_ui.backdrop_win, pw, ph);
    if (settings_ui.backdrop_buf != None)
      XFreePixmap (dpy, settings_ui.backdrop_buf);
      settings_ui.backdrop_buf = XCreatePixmap (dpy, root_win, pw, ph, depth);

    XMoveResizeWindow (dpy, settings_ui.win, panel_x, 0, panel_w, panel_h);
    s_switch_renderer (settings_ui.win, panel_w, panel_h);
  }

  settings_ui.width    = panel_w;
  settings_ui.height   = panel_h;
  settings_ui.parent_w = pw;
  settings_ui.parent_h = ph;

  read_settings_from_term (this);
  init_font_list(dpy);

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

  /* Restore default backing store now that the backdrop is gone.
     Exception: leave backing_store = Always on any vt whose minimap is
     active — render_minimap reads the obscured minimap strip via XCopyArea
     and needs the server to retain that content. */
  {
    XSetWindowAttributes bsa;
    bsa.backing_store = NotUseful;
    for (rxvt_term *t : termlist) {
#ifdef ENABLE_MINIMAP
      if (!t->minimap.enabled || t->minimap.win == None)
#endif
        XChangeWindowAttributes (dpy, t->vt, CWBackingStore, &bsa);
      if (t->split_partner && !t->split_is_child)
        XChangeWindowAttributes (dpy, t->split_partner->parent, CWBackingStore, &bsa);
    }
  }

  XFlush (dpy);

  /* Ensure backdrop is fully unmapped before restoring minimap */
  XSync(dpy, False);

  /* Restore minimap transparency after hiding settings UI */
#ifdef ENABLE_MINIMAP
  for (rxvt_term *t : termlist) {
    if (t->minimap.enabled && t->minimap.win != None) {
      XRaiseWindow(dpy, t->minimap.win);
      XSetWindowAttributes attr;
      attr.background_pixmap = ParentRelative;
      XChangeWindowAttributes(dpy, t->minimap.win, CWBackPixmap, &attr);
      XClearArea(dpy, t->minimap.win, 0, 0, 0, 0, False);
      t->render_minimap();
    }
  }
#endif

  /* Repaint only the active pane now that the backdrop is gone.
     Touching non-active tabs can cause issues (e.g., stale selection state
     on split panes that weren't visible). */
  GET_R->scr_touch (true);

  /* Reset microui state so the panel opens clean next time (no lingering
     open popups, stale focus, scroll positions, etc.). */
  if (mu_ctx) {
    mu_init (mu_ctx);
    mu_ctx->text_width  = text_width_cb;
    mu_ctx->font_height = font_height_cb;
  }
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
    /* Detach renderer before destroying the window it's bound to, so that
       r_switch_window() won't try to XRenderFreePicture a stale picture ID
       after the window is gone (which causes RenderBadPicture). */
    if (s_renderer_ok && s_renderer_win == settings_ui.win) {
      r_detach ();
      s_renderer_win = None;
    }
    XDestroyWindow (dpy, settings_ui.win);
    settings_ui.win = None;

    if (mu_ctx) { free (mu_ctx); mu_ctx = nullptr; }

    for (int i = 0; i < s_num_fonts; i++) {
      free (s_font_entries[i].name);
      free (s_font_entries[i].xlfd);
    }
    free (s_font_entries);
    s_font_entries = nullptr;
    s_num_fonts = 0;
    free(s_active_font_xlfd);
    s_active_font_xlfd = nullptr;
    s_active_font = -1;
    s_pending_font = -1;
    free(s_active_bold_xlfd);
    s_active_bold_xlfd = nullptr;
    s_active_bold_font = -1;
    s_pending_bold_font = -1;
  }
  settings_ui.visible = false;
}

/* ======================================================================
   Persist settings to ~/.Xdefaults
   - Strips any existing managed block (BEGIN/END markers)
   - Comments out any Exoterm.* / URxvt.* lines above the block that
     overlap with the keys we manage, to prevent duplicates
   - Appends a fresh managed block at the end of the file
   ====================================================================== */

#define XDEF_BLOCK_BEGIN "! --- BEGIN Exoterm settings ---"
#define XDEF_BLOCK_END   "! --- END Exoterm settings ---"

static const char *s_managed_keys[] = {
  "internalBorder", "shading", "lineSpace", "letterSpace", "saveLines",
  "wheelScrollLines", "scrollBar", "cursorBlink", "cursorUnderline",
  "scrollTtyOutput", "scrollTtyKeypress", "jumpScroll", "visualBell",
  "urgentOnBell", "mouseWheelScrollPage", "pointerBlank",
  "font", "boldFont", "foreground", "background",
  "color0",  "color1",  "color2",  "color3",  "color4",  "color5",  "color6",  "color7",
  "color8",  "color9",  "color10", "color11", "color12", "color13", "color14", "color15",
  "loginShell", "geometry", "cursorColor", "skipBuiltinGlyphs",
  "inheritPixmap", "transparent", "autoCopySelection",
  nullptr
};

static bool line_matches_managed_key (const char *line) {
  while (*line == ' ' || *line == '\t') line++;
  const char *prefixes[] = { "Exoterm.", "URxvt.", "exoterm.", "urxvt.", nullptr };
  for (int pi = 0; prefixes[pi]; pi++) {
    size_t plen = strlen (prefixes[pi]);
    if (strncmp (line, prefixes[pi], plen) != 0) continue;
    const char *rest = line + plen;
    for (int ki = 0; s_managed_keys[ki]; ki++) {
      size_t klen = strlen (s_managed_keys[ki]);
      if (strncmp (rest, s_managed_keys[ki], klen) != 0) continue;
      char c = rest[klen];
      if (c == ':' || c == ' ' || c == '\t' || c == '\0') return true;
    }
  }
  return false;
}

static void save_to_xdefaults (rxvt_term *first_term) {
  const char *home = getenv ("HOME");
  if (!home) return;

  char path[1024];
  snprintf (path, sizeof (path), "%s/.Xdefaults", home);

  /* --- read existing file into lines array --- */
  GPtrArray *lines = g_ptr_array_new_with_free_func (g_free);
  FILE *fr = fopen (path, "r");
  if (fr) {
    char buf[4096];
    while (fgets (buf, sizeof (buf), fr)) {
      size_t len = strlen (buf);
      if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
      g_ptr_array_add (lines, g_strdup (buf));
    }
    fclose (fr);
  }

  /* --- rebuild: strip existing block, comment conflicting keys --- */
  GPtrArray *out = g_ptr_array_new_with_free_func (g_free);
  bool in_block = false;
  for (guint i = 0; i < lines->len; i++) {
    const char *line = (const char *) lines->pdata[i];
    if (strcmp (line, XDEF_BLOCK_BEGIN) == 0) { in_block = true;  continue; }
    if (strcmp (line, XDEF_BLOCK_END)   == 0) { in_block = false; continue; }
    if (in_block) continue;

    // if (line_matches_managed_key (line))
    //   g_ptr_array_add (out, g_strdup_printf ("! %s", line));
    // else
      g_ptr_array_add (out, g_strdup (line));
  }

  g_ptr_array_unref (lines);

  /* --- build the new managed block --- */
  GString *block = g_string_new (XDEF_BLOCK_BEGIN "\n");

  g_string_append_printf (block, "Exoterm.loginShell:         %s\n", s_login_shell     ? "true" : "false");

  if (s_geometry[0])
    g_string_append_printf (block, "Exoterm.geometry:           %s\n", s_geometry);

  g_string_append_printf (block, "Exoterm.internalBorder:   %d\n",  (int)s_border_width);
  // g_string_append_printf (block, "Exoterm.shading:           %d\n",  (int)s_shading);

  g_string_append_printf (block, "Exoterm.lineSpace:         %d\n",  (int)s_line_space);
  // g_string_append_printf (block, "Exoterm.letterSpace:       %d\n",  (int)s_letter_space);

#ifdef BUILTIN_GLYPHS
  g_string_append_printf (block, "Exoterm.skipBuiltinGlyphs:  %s\n", s_skip_builtin_glyphs ? "true" : "false");
#endif

  if (s_active_font_xlfd && *s_active_font_xlfd)
    g_string_append_printf (block, "Exoterm.font:              %s\n", s_active_font_xlfd);
  if (s_active_bold_xlfd && *s_active_bold_xlfd)
    g_string_append_printf (block, "Exoterm.boldFont:          %s\n", s_active_bold_xlfd);

  // cursor and selection
  g_string_append_printf (block, "Exoterm.autoCopySelection:  %s\n", s_auto_copy_sel   ? "true" : "false");
  if (s_cursor_color[0])
    g_string_append_printf (block, "Exoterm.cursorColor:        %s\n", s_cursor_color);

  g_string_append_printf (block, "Exoterm.cursorBlink:        %s\n", s_cursor_blink        ? "true" : "false");
  g_string_append_printf (block, "Exoterm.cursorUnderline:    %s\n", s_cursor_underline    ? "true" : "false");

  // scroll
  g_string_append_printf (block, "Exoterm.saveLines:         %d\n",  (int)s_save_lines);
  g_string_append_printf (block, "Exoterm.wheelScrollLines:  %d\n",  (int)s_scroll_speed);
  g_string_append_printf (block, "Exoterm.scrollBar:          %s\n", s_scrollbar           ? "true" : "false");
  g_string_append_printf (block, "Exoterm.scrollTtyOutput:    %s\n", s_scroll_on_output    ? "true" : "false");
  g_string_append_printf (block, "Exoterm.scrollTtyKeypress:  %s\n", s_scroll_on_keypress  ? "true" : "false");
  g_string_append_printf (block, "Exoterm.jumpScroll:         %s\n", s_jump_scroll         ? "true" : "false");
  g_string_append_printf (block, "Exoterm.mouseWheelScrollPage: %s\n", s_mouse_wheel_page  ? "true" : "false");

  // alerts
  g_string_append_printf (block, "Exoterm.visualBell:         %s\n", s_visual_bell         ? "true" : "false");
  g_string_append_printf (block, "Exoterm.urgentOnBell:       %s\n", s_urgent_on_bell      ? "true" : "false");

#ifdef POINTER_BLANK
  g_string_append_printf (block, "Exoterm.pointerBlank:       %s\n", s_pointer_blank       ? "true" : "false");
#endif


// #ifdef HAVE_BG_PIXMAP
//   g_string_append_printf (block, "Exoterm.inheritPixmap:      %s\n", s_transparent     ? "true" : "false");
//   g_string_append_printf (block, "Exoterm.transparent:        %s\n", s_transparent     ? "true" : "false");
// #endif


  if (first_term) {
    char col[16];
    color_to_hex (first_term, Color_fg, col);
    g_string_append_printf (block, "Exoterm.foreground:        %s\n", col);
    color_to_hex (first_term, Color_bg, col);
    g_string_append_printf (block, "Exoterm.background:        %s\n", col);
    for (int i = 0; i < 16; i++) {
      color_to_hex (first_term, minCOLOR + i, col);
      g_string_append_printf (block, "Exoterm.color%-2d:           %s\n", i, col);
    }
  }

  g_string_append (block, XDEF_BLOCK_END "\n");

  /* --- trim trailing blank lines before appending block --- */
  while (out->len > 0) {
    const char *last = (const char *) out->pdata[out->len - 1];
    if (last[0] == '\0')
      g_ptr_array_remove_index (out, out->len - 1);
    else
      break;
  }

  /* --- write file --- */
  FILE *fw = fopen (path, "w");
  if (fw) {
    for (guint i = 0; i < out->len; i++)
      fprintf (fw, "%s\n", (const char *) out->pdata[i]);
    if (out->len > 0)
      fputc ('\n', fw);
    fputs (block->str, fw);
    fclose (fw);
  }

  g_string_free (block, TRUE);
  g_ptr_array_unref (out);
}

#undef XDEF_BLOCK_BEGIN
#undef XDEF_BLOCK_END

void
rxvt_term::draw_settings_ui ()
{
  if (!settings_ui.visible || !mu_ctx) return;

  s_switch_renderer (settings_ui.win, settings_ui.width, settings_ui.height);

  mu_begin (mu_ctx);
  int changed = build_settings_window (mu_ctx);
  mu_end (mu_ctx);

  r_clear (mu_color (0x34, 0x35, 0x38, 255));
  render_mu (mu_ctx);
  r_present ();

  /* If popup state changed this frame (opened/closed), kick the refresh
     timer to fire immediately so the transition renders without waiting
     up to ~33ms for the next scheduled tick. Equivalent to sokol's
     sapp_wait_for_event(false) when needs_redraw is set. */
  if (mu_ctx->needs_redraw) {
    settings_ui_refresh_ev.stop ();
    settings_ui_refresh_ev.set (0.0, 1.0 / 30.0);
    settings_ui_refresh_ev.start ();
  }

  if (changed & CHANGED_CLOSE) {
    destroy_settings_ui ();
    return;
  }

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

  if (changed & CHANGED_SAVE) {
    rxvt_term *t = rxvt_term::termlist.empty () ? nullptr : rxvt_term::termlist.front ();
    save_to_xdefaults (t);
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

/* ======================================================================
   Context menu — microui popup that appears on right-click
   ====================================================================== */

/* Build the context menu contents; returns the action id (CM_*) or CM_NONE. */
static int build_context_menu (mu_Context *ctx, rxvt_term *t, int w, int h)
{
  int action = CM_NONE;
  bool has_split = (t->split_partner != nullptr);

  int opt = MU_OPT_NORESIZE | MU_OPT_NODRAG | MU_OPT_NOTITLE
          | MU_OPT_NOFRAME | MU_OPT_NOSCROLL;

  if (!mu_begin_window_ex (ctx, "##ctxmenu", mu_rect (0, 0, w, h), opt))
    return CM_NONE;

  int col[] = {-1};

  mu_layout_row (ctx, 1, col, CM_ITEM_H);
  if (mu_button_ex (ctx, "New Tab",    0, MU_OPT_BURIED))     action = CM_NEW_TAB;
  if (mu_button_ex (ctx, "New Window", 0, MU_OPT_BURIED))     action = CM_NEW_WINDOW;

  mu_menu_separator (ctx);

  mu_layout_row (ctx, 1, col, CM_ITEM_H);
  if (mu_button_ex (ctx, "Copy",  0, MU_OPT_BURIED))          action = CM_COPY;
  if (mu_button_ex (ctx, "Paste", 0, MU_OPT_BURIED))          action = CM_PASTE;

  mu_menu_separator (ctx);

  mu_layout_row (ctx, 1, col, CM_ITEM_H);
  if (has_split) {
    if (mu_button_ex (ctx, "Close Split", 0, MU_OPT_BURIED))  action = CM_CLOSE_SPLIT;
  } else {
    if (mu_button_ex (ctx, "Split Horizontally", 0, MU_OPT_BURIED)) action = CM_SPLIT_H;
    if (mu_button_ex (ctx, "Split Vertically",   0, MU_OPT_BURIED)) action = CM_SPLIT_V;
  }

#ifdef ENABLE_MINIMAP
  if (t->minimap.enabled) {
    // mu_menu_separator (ctx);
    // mu_layout_row (ctx, 1, col, CM_ITEM_H);
    if (mu_button_ex (ctx, "Toggle Minimap", 0, MU_OPT_BURIED)) action = CM_TOGGLE_MINIMAP;
  }
#endif

  mu_menu_separator (ctx);

  mu_layout_row (ctx, 1, col, CM_ITEM_H);
  if (mu_button_ex (ctx, "Show Settings", 0, MU_OPT_BURIED))  action = CM_SHOW_SETTINGS;

  mu_layout_row (ctx, 1, col, CM_ITEM_H);
  if (mu_button_ex (ctx, "Close Tab", 0, MU_OPT_BURIED))      action = CM_CLOSE_TAB;

  mu_end_window (ctx);
  return action;
}

void
rxvt_term::show_context_menu (int x_root, int y_root, rxvt_term *invoker)
{
  /* Only the root term owns the context-menu window and microui context. */
  if (this != termlist.at (0)) {
    termlist.at (0)->show_context_menu (x_root, y_root, invoker);
    return;
  }

  /* Don't overlap with the settings panel. */
  if (settings_ui.visible) return;

  /* Close any already-open context menu first. */
  if (context_menu.visible) hide_context_menu ();

  s_cm_invoker = invoker ? invoker : this;

  bool has_split  = (s_cm_invoker->split_partner != nullptr);
#ifdef ENABLE_MINIMAP
  bool has_minimap = s_cm_invoker->minimap.enabled;
#else
  bool has_minimap = false;
#endif
  context_menu.w = CM_W;
  context_menu.h = cm_compute_h (has_split, has_minimap);

  /* Adjust so the window fits on-screen. */
  int scr = DefaultScreen (dpy);
  int sw  = DisplayWidth  (dpy, scr);
  int sh  = DisplayHeight (dpy, scr);
  context_menu.x = x_root;
  context_menu.y = y_root;
  if (context_menu.x + context_menu.w > sw) context_menu.x = sw - context_menu.w;
  if (context_menu.y + context_menu.h > sh) context_menu.y = sh - context_menu.h;
  if (context_menu.x < 0) context_menu.x = 0;
  if (context_menu.y < 0) context_menu.y = 0;

  if (context_menu.win == None) {
    /* Create the popup window (override-redirect = no WM interference). */
    XSetWindowAttributes attr = {};
    attr.background_pixel  = BlackPixel (dpy, scr);
    attr.border_pixel      = 0;
    attr.colormap          = cmap;
    attr.override_redirect = True;

    context_menu.win = XCreateWindow (
      dpy, DefaultRootWindow (dpy),
      context_menu.x, context_menu.y, context_menu.w, context_menu.h, 0,
      depth, InputOutput, visual,
      CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect, &attr);

    if (!context_menu.win) return;

    context_menu.gc = XCreateGC (dpy, context_menu.win, 0, nullptr);
    context_menu_ev.start (display, context_menu.win);

    if (!mu_ctx_menu) {
      mu_ctx_menu = (mu_Context *) malloc (sizeof (mu_Context));
      mu_init (mu_ctx_menu);
      mu_ctx_menu->text_width  = text_width_cb;
      mu_ctx_menu->font_height = font_height_cb;
    }

    /* Initialize or switch the shared renderer to this window. */
    if (!s_renderer_ok) {
      r_init (dpy, context_menu.win, context_menu.gc, visual, depth,
              context_menu.w, context_menu.h);
      s_renderer_ok  = true;
      s_renderer_win = context_menu.win;
    } else {
      s_switch_renderer (context_menu.win, context_menu.w, context_menu.h);
    }

  } else {
    /* Reuse existing window — move/resize it. */
    XMoveResizeWindow (dpy, context_menu.win,
                       context_menu.x, context_menu.y,
                       context_menu.w, context_menu.h);
    s_switch_renderer (context_menu.win, context_menu.w, context_menu.h);
  }

  context_menu.visible = true;

  XMapRaised (dpy, context_menu.win);

  /* Grab the pointer so out-of-bounds clicks reach our window and close it. */
  XGrabPointer (dpy, context_menu.win, False,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

  /* Warm up microui mouse position to avoid stale hover. */
  mu_input_mousemove (mu_ctx_menu, context_menu.w / 2, context_menu.h / 2);

  XFlush (dpy);
  draw_context_menu ();
}

void
rxvt_term::hide_context_menu ()
{
  if (!context_menu.visible) return;
  context_menu.visible = false;

  XUngrabPointer (dpy, CurrentTime);

  if (context_menu.win != None)
    XUnmapWindow (dpy, context_menu.win);

  /* Reset microui so next open is clean. */
  if (mu_ctx_menu) {
    mu_init (mu_ctx_menu);
    mu_ctx_menu->text_width  = text_width_cb;
    mu_ctx_menu->font_height = font_height_cb;
  }

  s_cm_invoker = nullptr;

  /* Return input focus to the active terminal pane. */
  XSetInputFocus (dpy, GET_R->parent, RevertToPointerRoot, CurrentTime);
  XFlush (dpy);
}

void
rxvt_term::destroy_context_menu ()
{
  /* Only meaningful on the root term (the only one that owns context_menu.win). */
  if (context_menu.win == None) return;

  context_menu_ev.stop (display);

  if (context_menu.visible) {
    XUngrabPointer (dpy, CurrentTime);
    context_menu.visible = false;
  }

  s_cm_invoker = nullptr;

  if (mu_ctx_menu) { free (mu_ctx_menu); mu_ctx_menu = nullptr; }

  if (context_menu.gc != None) {
    XFreeGC (dpy, context_menu.gc);
    context_menu.gc = None;
  }

  if (s_renderer_ok && s_renderer_win == context_menu.win) {
    r_detach ();
    s_renderer_win = None;
  }

  XDestroyWindow (dpy, context_menu.win);
  context_menu.win = None;
}

void
rxvt_term::draw_context_menu ()
{
  if (!context_menu.visible || !mu_ctx_menu || !s_cm_invoker) return;

  s_switch_renderer (context_menu.win, context_menu.w, context_menu.h);

  mu_begin (mu_ctx_menu);
  int action = build_context_menu (mu_ctx_menu, s_cm_invoker,
                                   context_menu.w, context_menu.h);
  mu_end (mu_ctx_menu);

  r_clear (mu_color (0x2c, 0x2d, 0x30, 255));
  render_mu (mu_ctx_menu);
  r_present ();

  if (action == CM_NONE) return;

  /* An item was clicked — close first, then act. */
  rxvt_term *t = s_cm_invoker;
  hide_context_menu ();

  switch (action) {
    case CM_NEW_TAB:
      t->new_tab ();
      break;

    case CM_NEW_WINDOW:
      t->new_window ();
      break;

    case CM_COPY:
      if (t->selection.len > 0) {
        free (t->selection.clip_text);
        t->selection.clip_text = (wchar_t *) malloc (
          (t->selection.len + 1) * sizeof (wchar_t));
        if (t->selection.clip_text) {
          wmemcpy (t->selection.clip_text, t->selection.text, t->selection.len);
          t->selection.clip_text[t->selection.len] = 0;
          t->selection.clip_len = t->selection.len;
          t->selection_grab (CurrentTime, true);
        }
      }
      break;

    case CM_PASTE:
      t->selection_request (CurrentTime, Sel_Clipboard);
      break;

    case CM_SPLIT_H:
      t->new_split_pane (false);
      break;

    case CM_SPLIT_V:
      t->new_split_pane (true);
      break;

    case CM_CLOSE_SPLIT: {
      /* Always destroy the child pane to restore single-pane view. */
      rxvt_term *child = t->split_is_child ? t : t->split_partner;
      if (child) child->close_tab ();
      break;
    }

    case CM_SHOW_SETTINGS:
      termlist.at (0)->show_settings_ui ();
      break;

#ifdef ENABLE_MINIMAP
    case CM_TOGGLE_MINIMAP:
      t->minimap.visible = !t->minimap.visible;
      if (t->minimap.visible)
        XMapWindow  (t->dpy, t->minimap.win);
      else
        XUnmapWindow (t->dpy, t->minimap.win);
      break;
#endif

    case CM_CLOSE_TAB:
      t->close_tab ();
      break;
  }
}

void
rxvt_term::x_context_menu_cb (XEvent &xev)
{
  if (!mu_ctx_menu || !context_menu.visible) return;

  static int cm_mx = 0, cm_my = 0;

  switch (xev.type) {
    case MotionNotify:
      cm_mx = xev.xmotion.x;
      cm_my = xev.xmotion.y;
      mu_input_mousemove (mu_ctx_menu, cm_mx, cm_my);
      draw_context_menu ();
      break;

    case ButtonPress:
      cm_mx = xev.xbutton.x;
      cm_my = xev.xbutton.y;

      /* Click outside the menu bounds → dismiss. */
      if (cm_mx < 0 || cm_mx >= context_menu.w ||
          cm_my < 0 || cm_my >= context_menu.h) {
        hide_context_menu ();
        return;
      }

      if (xev.xbutton.button == Button1)
        mu_input_mousedown (mu_ctx_menu, cm_mx, cm_my, MU_MOUSE_LEFT);
      else if (xev.xbutton.button == Button3)
        mu_input_mousedown (mu_ctx_menu, cm_mx, cm_my, MU_MOUSE_RIGHT);

      draw_context_menu ();
      break;

    case ButtonRelease:
      cm_mx = xev.xbutton.x;
      cm_my = xev.xbutton.y;

      if (xev.xbutton.button == Button1)
        mu_input_mouseup (mu_ctx_menu, cm_mx, cm_my, MU_MOUSE_LEFT);
      else if (xev.xbutton.button == Button3)
        mu_input_mouseup (mu_ctx_menu, cm_mx, cm_my, MU_MOUSE_RIGHT);

      draw_context_menu ();
      break;

    default:
      break;
  }
}
