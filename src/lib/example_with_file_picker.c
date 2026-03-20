// build with:
// cc -o example example.c microui_renderer_xrender.c microui.c -lm -lX11 -lXrender

#define MICROUI_FILE_PICKER_IMPLEMENTATION

#include "microui.h"
#include "microui_file_picker.h"
#include "microui_renderer_xrender.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

SofdState* sofd;
char selected_file[1024];
int show_dialog;

static float bg[3] = { 90, 95, 100 };


/* Callback for file selection */
void file_selected_callback(const char** files, int count, void* userdata) {
    if (count == 1) {
        strcpy(selected_file, files[0]);
        printf("Selected: %s\n", files[0]);
    } else {
        printf("Selected %d files:\n", count);
        for (int i = 0; i < count; i++) {
            printf("  %s\n", files[i]);
        }
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

  /* Initialize file dialog */
  sofd = sofd_new();
  sofd_set_title(sofd, "Select a File");
  sofd_set_filter(sofd, "*.txt;*.c;*.h");
  sofd_set_callback(sofd, file_selected_callback, NULL);

  int fps = 60;

  /* main loop */
  for (;;) {
    int64_t before = r_get_time();

    if (r_handle_input(ctx)) { // true if should close
      break;
    }

    // if (r_needs_redraw()) {
        mu_begin(ctx);

        if (mu_begin_window(ctx, "Main", mu_rect(50, 50, 300, 200))) {
            mu_label(ctx, selected_file[0] ? selected_file : "No file selected", 0);

            if (mu_button(ctx, "Open File")) {
                sofd_set_mode(sofd, SOFD_MODE_OPEN_FILE);
                sofd_show(sofd);
            }

            mu_layout_row(ctx, 2, (int[]) { 100, 100 }, 0);
            if (mu_button(ctx, "Open Multiple")) {
                sofd_set_mode(sofd, SOFD_MODE_OPEN_MULTIPLE);
                sofd_show(sofd);
            }

            if (mu_button(ctx, "Select Directory")) {
                sofd_set_mode(sofd, SOFD_MODE_SELECT_DIR);
                sofd_show(sofd);
            }

            mu_end_window(ctx);
        }

        sofd_render(ctx, sofd);
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

  sofd_free(sofd);
  free(ctx);

  XFreeGC(dpy, gc);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
  return 0;
}
