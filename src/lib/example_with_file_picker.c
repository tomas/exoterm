// build with:
// cc -o example example_with_file_picker.c microui_renderer_xrender.c microui.c -lm -lX11 -lXrender

#define MICROUI_FILE_PICKER_IMPLEMENTATION

#include "microui.h"
#include "microui_file_picker.h"
#include "microui_renderer_xrender.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static float bg[3] = { 90, 95, 100 };

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

static int audio_filter(const char *name)
{
    /* show only audio file formats – mirrors the sofd screenshot */
    const char *exts[] = { ".wav", ".aiff", ".flac", ".ogg", ".mp3",
                           ".opus", ".w64",  ".caf",  NULL };
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    for (int i = 0; exts[i]; i++)
        if (!strcasecmp(dot, exts[i])) return 1;
    return 0;
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

  MuFilePicker fp;
  const char *start_dir = getenv("HOME");
  mu_filepicker_init(&fp, start_dir ? start_dir : "/");

  /* Optional: attach audio filter (List All Files toggle will bypass it) */
  mu_filepicker_set_filter(&fp, audio_filter);

  /* Optional: pre-populate recently used list */
  mu_fp_add_recent(&fp, "/home/rgareus/Freesound/snd/161425-Guitar chord.wav", 0);
  mu_fp_add_recent(&fp, "/home/rgareus/Freesound/snd/161697-ocean-1.wav",      0);

  int fps = 60;

  /* main loop */
  for (;;) {
    int64_t before = r_get_time();

    if (r_handle_input(ctx)) { // true if should close
      break;
    }

    // if (r_needs_redraw()) {
        mu_begin(ctx);

        mu_filepicker_draw(&fp, ctx);

        /* --- poll result --- */
        int st = mu_filepicker_status(&fp);
        if (st == MU_FP_SELECTED) {
            printf("Selected file: %s\n", mu_filepicker_filename(&fp));
            /* add to recently-used list */
            mu_fp_add_recent(&fp, mu_filepicker_filename(&fp), 0);
            /* reset for next use, or close your app: */
            mu_filepicker_reset(&fp);
            /* mu_filepicker_init(&fp, new_start_dir); */
        } else if (st == MU_FP_CANCELLED) {
            printf("Cancelled.\n");
            mu_filepicker_reset(&fp);
            // running = 0;   /* or keep the app open */
        }

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
