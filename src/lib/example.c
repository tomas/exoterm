// build with:
// cc -o example example.c microui_renderer_xrender.c microui.c -lm -lX11 -lXrender

#include "microui.h"
#include "microui_renderer_xrender.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static float bg[3] = { 90, 95, 100 };

static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
  mu_pop_id(ctx);
  return res;
}

static const char *fruits[] = {"Apple", "Banana", "Cherry", "Date", "Elderberry"};
static int num_fruits = 5;
int selected_fruit = 0;

static void style_window(mu_Context *ctx) {
  static struct { const char *label; int idx; } colors[] = {
    { "text:",         MU_COLOR_TEXT        },
    { "border:",       MU_COLOR_BORDER      },
    { "windowbg:",     MU_COLOR_WINDOWBG    },
    { "titlebg:",      MU_COLOR_TITLEBG     },
    { "titletext:",    MU_COLOR_TITLETEXT   },
    { "panelbg:",      MU_COLOR_PANELBG     },
    { "button:",       MU_COLOR_BUTTON      },
    { "buttonhover:",  MU_COLOR_BUTTONHOVER },
    { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
    { "base:",         MU_COLOR_BASE        },
    { "basehover:",    MU_COLOR_BASEHOVER   },
    { "basefocus:",    MU_COLOR_BASEFOCUS   },
    { "scrollbase:",   MU_COLOR_SCROLLBASE  },
    { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
    { NULL }
  };

  if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {

    mu_layout_row(ctx, 1, (int[]){150}, 0);

    const char *current = fruits[selected_fruit];
    if (mu_begin_combo_ex(ctx, "##fruit", current, num_fruits, 0)) {
      mu_layout_row(ctx, 1, (int[]){-1}, 0);
      for (int i = 0; i < num_fruits; i++) {
        if (mu_button(ctx, fruits[i])) {
          selected_fruit = i;
          mu_close_popup(ctx, "##fruit");
        }
      }
      mu_end_combo(ctx);
    }

    int sw = mu_get_current_container(ctx)->body.w * 0.14;
    mu_layout_row(ctx, 6, (int[]) { 80, sw, sw, sw, sw, -1 }, 0);
    for (int i = 0; colors[i].label; i++) {
      mu_label(ctx, colors[i].label, 0);
      uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
      mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
    }
    mu_end_window(ctx);
  }
}

static int text_width(mu_Font font, const char *text, int len) {
  if (len == -1) { len = strlen(text); }
  return r_get_text_width(text, len);
}

static int font_height(mu_Font font) {
  return r_get_text_height();
}

static int mousex = 0, mousey = 0;

static int r_handle_input(mu_Context *ctx) {
  if (r_mouse_moved(&mousex, &mousey)) {
    mu_input_mousemove(ctx, mousex, mousey);
  }
  if (r_mouse_down()) {
    mu_input_mousedown(ctx, mousex, mousey, MU_MOUSE_LEFT);
  } else if (r_mouse_up()) {
    mu_input_mouseup(ctx, mousex, mousey, MU_MOUSE_LEFT);
  }

  if (r_should_close() || r_key_down(0x1b)) { // close button or esc
    return 1;
  }

  if (r_key_down('\n')) { mu_input_keydown(ctx, MU_KEY_RETURN); }
  else if (r_key_up('\n')) { mu_input_keyup(ctx, MU_KEY_RETURN); }
  if (r_key_down('\b')) { mu_input_keydown(ctx, MU_KEY_BACKSPACE); }
  else if (r_key_up('\b')) { mu_input_keyup(ctx, MU_KEY_BACKSPACE); }
  for (int i = 0; i < 256; i++) {
    if (r_key_down(i)) {
      if (' ' <= i  &&  i <= '~') {
        char text[2] = {i, 0};
        if (isalpha(i)) {
          if (!r_shift_pressed()) {
            text[0] = tolower(i);
          }
        }
        else {
          // TODO(kartik): depends on keyboard layout
        }
        mu_input_text(ctx, text);
      }
      continue;
    }
    // TODO(max): mod
  }

  return 0;
}

static void r_render(mu_Context *ctx) {
  mu_Command *cmd = NULL;
  while (mu_next_command(ctx, &cmd)) {
    switch (cmd->type) {
      case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
      case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
      case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
      case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
    }
  }

  r_present();
  r_process_events();
}

int main(int argc, char **argv) {

  int width = 800, height = 600;
  Display *dpy = XOpenDisplay(NULL);
  int screen   = DefaultScreen(dpy);
  Window win   = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, width, height, 0, BlackPixel(dpy, screen), WhitePixel(dpy, screen));
  GC gc        = XCreateGC(dpy, win, 0, NULL);
  XStoreName(dpy, win, "microui demo");
  XMapWindow(dpy, win);
  XSync(dpy, False);

  /* init renderer and microui */
  int res = r_init(dpy, win, gc, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen), width, height);
  if (res != 0) {
    // exit(1);
    return 1;
  }

  mu_Context *ctx = malloc(sizeof(mu_Context));
  mu_init(ctx);
  ctx->text_width = text_width;
  ctx->font_height = font_height;

  int fps = 60;

  /* main loop */
  for (;;) {
    int64_t before = r_get_time();

    if (r_handle_input(ctx)) { // true if should close
      break;
    }

    // if (r_needs_redraw()) {
        /* Force a full redraw if needed (expose event occurred) */
        mu_begin(ctx);
        style_window(ctx);
        mu_end(ctx);
    // }

    r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    r_render(ctx);

    int64_t after = r_get_time();
    int64_t paint_time_ms = after - before;
    int64_t frame_budget_ms = 1000 / fps;
    int64_t sleep_time_ms = frame_budget_ms - paint_time_ms;
    if (sleep_time_ms > 0) {
      r_sleep(sleep_time_ms);
    }
  }

  free(ctx);
  XFreeGC(dpy, gc);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
  return 0;
}
