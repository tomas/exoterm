/*
** Copyright (c) 2020 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "microui.h"

#define unused(x) ((void) (x))

#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)

#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* incremented after incase `val` uses this value */       \
  } while (0)

#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)

// static mu_Id active_popup_id = 0;
static int context_menu_x = 0;
static int context_menu_y = 0;

static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

static mu_Style default_style = {
  /* font | size | padding | spacing | indent */
  NULL, { 68, 10 }, 8, 6, 24,
  /* title_height | scrollbar_size | thumb_size */
  28, 12, 8,
  {
    { 230, 230, 230, 255 }, /* MU_COLOR_TEXT */
    { 25,  25,  25,  255 }, /* MU_COLOR_BORDER */
    { 50,  50,  50,  255 }, /* MU_COLOR_WINDOWBG */
    { 25,  25,  25,  255 }, /* MU_COLOR_TITLEBG */
    { 240, 240, 240, 255 }, /* MU_COLOR_TITLETEXT */
    { 0,   0,   0,   0   }, /* MU_COLOR_PANELBG */
    { 75,  75,  75,  255 }, /* MU_COLOR_BUTTON */
    { 95,  95,  95,  255 }, /* MU_COLOR_BUTTONHOVER */
    { 115, 115, 115, 255 }, /* MU_COLOR_BUTTONFOCUS */
    { 30,  30,  30,  255 }, /* MU_COLOR_BASE */
    { 35,  35,  35,  255 }, /* MU_COLOR_BASEHOVER */
    { 40,  40,  40,  255 }, /* MU_COLOR_BASEFOCUS */
    { 43,  43,  43,  255 }, /* MU_COLOR_SCROLLBASE */
    { 30,  30,  30,  255 }, /* MU_COLOR_SCROLLTHUMB */
    { 50,  50,  50,  255 }, /* MU_COLOR_SCROLLFOCUS */
    { 75,  75,  75,  255 }, /* MU_COLOR_HANDLE */
    { 95,  95,  95,  255 }, /* MU_COLOR_HANDLEHOVER */
    { 115, 115, 115, 255 }, /* MU_COLOR_HANDLEFOCUS */
    { 75,  75,  75,  255 }, /* MU_COLOR_TABCONTENT */
    { 75,  75,  75,  255 }, /* MU_COLOR_TABACTIVE */
    { 65,  65,  65,  255 }, /* MU_COLOR_TABINACTIVE */
    { 95,  95,  95,  255 }, /* MU_COLOR_TABHOVER */
  }
};

#ifndef bool
#define bool int
#endif

int num_chars(char *s) {
  int len = 0;
  for (; *s; ++s) { if ((*s & 0xC0) != 0x80) ++len; }
  return len;
}

int num_bytes(const char *s) {
  int len = 0;
  for (; *s; ++s) ++len;
  return len;
}

int num_bytes_until(const char * str, int bytelen, int from_pos, int to_pos) {
  int c, i = 0;
  for (c = from_pos; c < to_pos; c++) {
    while ((str[++i] & 0xc0) == 0x80 && i < bytelen);
  }
  return i;
}

mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; res.y = y;
  return res;
}


mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; res.y = y; res.w = w; res.h = h;
  return res;
}

mu_TextureRect mu_texture_rect(float x, float y, float w, float h) {
  mu_TextureRect res;
  res.x = x; res.y = y; res.w = w; res.h = h;
  return res;
}

mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; res.g = g; res.b = b; res.a = a;
  return res;
}


static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

// static mu_Rect expand_rect_ex(mu_Rect rect, uint top, uint bottom, uint left, uint right) {
//   return mu_rect(rect.x - left, rect.y - top, rect.w + (left + right), rect.h + (top + bottom));
// }

// static mu_Rect expand_rect_geo(mu_Rect rect, uint n, uint nw, uint ne, uint se, uint sw) {
//   return expand_rect_ex(rect, nw || ne ? n : 0, sw || se ? n : 0 , nw || sw ? n : 0, ne || se ? n : 0);
// }

static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}


static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}


static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_SCROLLFOCUS ||
      colorid == MU_COLOR_TABACTIVE ||
      colorid == MU_COLOR_TITLEBG) { return; } // don't draw border on these

  /* draw border */
  if (ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}


void mu_init(mu_Context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->draw_frame = draw_frame;
  ctx->_style = default_style;
  ctx->style = &ctx->_style;
}


void mu_begin(mu_Context *ctx) {
  expect(ctx->text_width && ctx->font_height);
  ctx->command_list.idx = 0;
  ctx->root_list.idx = 0;
  ctx->scroll_target = NULL;
  ctx->hover_root = ctx->next_hover_root;
  ctx->next_hover_root = NULL;
  ctx->mouse_delta.x = ctx->mouse_pos.x - ctx->last_mouse_pos.x;
  ctx->mouse_delta.y = ctx->mouse_pos.y - ctx->last_mouse_pos.y;
  ctx->frame++;
  ctx->needs_redraw = 0;
  ctx->copy_text[0] = '\0';
}


static int compare_zindex(const void *a, const void *b) {
  return (*(mu_Container**) a)->zindex - (*(mu_Container**) b)->zindex;
}


void mu_end(mu_Context *ctx) {
  int i, n;
  /* check stacks */
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx      == 0);
  expect(ctx->id_stack.idx        == 0);
  expect(ctx->layout_stack.idx    == 0);

  /* handle scroll input */
  // if (ctx->scroll_target) {
  if (ctx->scroll_target && !ctx->prevent_scroll) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  /* unset focus if focus id was not touched this frame */
  if (!ctx->updated_focus) { ctx->focus = 0; }
  ctx->updated_focus = 0;

  /* bring hover root to front if mouse was pressed */
  if (ctx->mouse_pressed && ctx->next_hover_root &&
      ctx->next_hover_root->zindex < ctx->last_zindex &&
      ctx->next_hover_root->zindex >= 0
  ) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  /* reset input state */
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  // ctx->copy_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->mouse_up = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;
  ctx->prevent_scroll = 0;

  /* sort root containers by zindex */
  n = ctx->root_list.idx;
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  for (int z = 1; z <= 2; z++) {
    for (i = 0; i < n; i++) {
      if (ctx->root_list.items[i]->zorder == z) {
        ctx->root_list.items[i]->zindex = ++ctx->last_zindex;
      }
    }
  }

  // re-sort after adjustments
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  /* set root container jump commands */
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];
    /* if this is the first container then make the first command jump to it.
    ** otherwise set the previous container's tail to jump to this one */
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }
    /* make the last container's tail jump to the end of command list */
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}


void mu_set_focus(mu_Context *ctx, mu_Id id) {
  // printf("setting last focus to %d\n", ctx->focus);
  ctx->last_focus = ctx->focus;
  ctx->focus = id;
  ctx->updated_focus = 1;
}


/* 32bit fnv-1a hash */
#define HASH_INITIAL 2166136261

static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619;
  }
}


mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx;
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size);
  ctx->last_id = res;
  return res;
}


void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}


void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}


void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}


void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}


mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}


int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y   ) { return MU_CLIP_ALL; }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h ) { return 0; }
  return MU_CLIP_PART;
}


static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}


static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}


static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  /* pop container, layout and id */
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}


mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ ctx->container_stack.idx - 1 ];
}


static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  /* try to get existing container from pool */

  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if (idx >= 0) {
    if (ctx->containers[idx].open || ~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }

    return &ctx->containers[idx];
  }

  if (opt & MU_OPT_CLOSED) { return NULL; }
  /* container not found in pool: init new container */
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
  return cnt;
}


mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}


void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) {
  cnt->zindex = ++ctx->last_zindex;
}

/*============================================================================
** pool
**============================================================================*/

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for (i = 0; i < len; i++) {
    if (items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1);
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}


int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx);
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1;
}


void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}


/*============================================================================
** input handlers
**============================================================================*/

#ifdef __linux__

static void mu_get_time(struct timespec * ts) {
  clock_gettime(CLOCK_MONOTONIC, ts);
}

static double mu_get_timediff(struct timespec start) {
  struct timespec end = { 0, 0 };
  mu_get_time(&end);

  return ((double)end.tv_sec + 1.0e-9 * end.tv_nsec) \
    - ((double)start.tv_sec + 1.0e-9 * start.tv_nsec);
}

#else

static void mu_get_time(struct timeval * ts) {
  gettimeofday(ts, NULL);
}

static double mu_get_timediff(struct timeval start) {
  struct timeval end = { 0, 0 };
  mu_get_time(&end);

  return ((double)end.tv_sec + 1e-6 * end.tv_usec) \
    - ((double)start.tv_sec + 1e-6 * start.tv_usec);
}

#endif

void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  ctx->mouse_pos = mu_vec2(x, y);
}

void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;

  if (ctx->mouse_down_ts.tv_sec) {
    ctx->last_click_time = mu_get_timediff(ctx->mouse_down_ts);
  }

  mu_get_time(&ctx->mouse_down_ts);
}

void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
  ctx->mouse_up |= btn;
}

void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}

void mu_input_keydown(mu_Context *ctx, int key) {
  ctx->key_pressed |= key;
  ctx->key_down |= key;
}

void mu_input_keyup(mu_Context *ctx, int key) {
  ctx->key_down &= ~key;
}

bool mu_is_key_pressed(mu_Context *ctx, int key) {
  return ctx->key_pressed & key;
}

bool mu_is_key_down(mu_Context *ctx, int key) {
  return ctx->key_down & key;
}

void mu_input_text(mu_Context *ctx, const char *text) {
  int current_len = strlen(ctx->input_text);
  // clamp text instead of simply exploding
  // int size = strlen(text) + 1;
  int size = mu_min(strlen(text) + 1, sizeof(ctx->input_text) - current_len);
  // expect(len + size <= (int)sizeof(ctx->input_text));
  memcpy(ctx->input_text + current_len, text, size);
}


/*============================================================================
** commandlist
**============================================================================*/

mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  const int al = sizeof(void*) - 1;
  size = (size + al) & ~al;
  expect(size % sizeof(void*) == 0);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}


int mu_next_command(mu_Context *ctx, mu_Command **cmd) {
  if (*cmd) {
    *cmd = (mu_Command*) (((char*) *cmd) + (*cmd)->base.size);
  } else {
    *cmd = (mu_Command*) ctx->command_list.items;
  }
  while ((char*) *cmd != ctx->command_list.items + ctx->command_list.idx) {
    if ((*cmd)->type != MU_COMMAND_JUMP) { return 1; }
    *cmd = (*cmd)->jump.dst;
  }
  return 0;
}


static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}


void mu_set_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}


void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}

void mu_draw_pixel(mu_Context *ctx, int x, int y, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(x, y, 1, 1), color);
}

void mu_draw_rect_corner_pixels_ex(mu_Context *ctx, mu_Rect rect, mu_Color color, uint nw, uint ne, uint se, uint sw) {
  if (nw) mu_draw_pixel(ctx, rect.x, rect.y, color);
  if (ne) mu_draw_pixel(ctx, rect.x+rect.w-1, rect.y, color);
  if (se) mu_draw_pixel(ctx, rect.x+rect.w-1, rect.y+rect.h-1, color);
  if (sw) mu_draw_pixel(ctx, rect.x, rect.y+rect.h-1, color);
}

void mu_draw_rect_corner_pixels(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect_corner_pixels_ex(ctx, rect, color, 1, 1, 1, 1);
}

void mu_draw_box_outer(mu_Context *ctx, mu_Rect rect, mu_Color color, uint width, uint roundness) {
  expect(width > 0);

  mu_draw_rect(ctx, mu_rect(rect.x - width + roundness, rect.y - width, rect.w + ((width - roundness) * 2), width), color); // top
  mu_draw_rect(ctx, mu_rect(rect.x - width + roundness, rect.y + rect.h, rect.w + ((width - roundness) * 2), width), color); // bottom
  mu_draw_rect(ctx, mu_rect(rect.x - width, rect.y - (width - roundness), width, rect.h + (width - roundness) * 2), color); // left
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w, rect.y - (width - roundness), width, rect.h + (width - roundness) * 2), color); // right
}

static void mu_draw_box_inner_ex(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness, uint wtop, uint wbottom, uint wleft, uint wright) {
  // expect(width > 0);

  if (wtop) mu_draw_rect(ctx, mu_rect(rect.x + roundness, rect.y, rect.w - (roundness * 2), wtop), color); // top
  if (wbottom) mu_draw_rect(ctx, mu_rect(rect.x + roundness, rect.y + rect.h - wbottom, rect.w - (roundness * 2), wbottom), color); // bottom

  if (wleft) mu_draw_rect(ctx, mu_rect(rect.x, rect.y + roundness, wleft, rect.h - (roundness * 2)), color); // left
  if (wright) mu_draw_rect(ctx, mu_rect(rect.x + rect.w - wright, rect.y + roundness, wright, rect.h - (roundness * 2)), color); // right
}

void mu_draw_box_inner(mu_Context *ctx, mu_Rect rect, mu_Color color, uint width, uint roundness) {
  expect(width > 0);

  mu_draw_box_inner_ex(ctx, rect, color, roundness, width, width, width, width);
}

void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_box_inner(ctx, rect, color, 1, 0);
}

static void mu_draw_rounded_box_margin_ex(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness, uint nw, uint ne, uint se, uint sw) {
  expect(roundness > 2 && roundness < 7);

  // draw border at 0px
  mu_draw_box_inner(ctx, rect, color, 1, roundness-1);

  // float alpha_per_step = 1/(roundness-1.0);
  float alpha_per_step = 1/(roundness-1.5); // make pixels a bit darker (+0.5)
  mu_Color col;
  int step;
  for (step = 1; step <= roundness-2; step++) {
    // printf("step: %d, alpha_per_step: %f\n", step, alpha_per_step);
    col = mu_color(color.r, color.g, color.b, (int)color.a*(step*(alpha_per_step)));

    if (nw) { // topleft
      mu_draw_pixel(ctx, rect.x, rect.y+step, col);
      mu_draw_pixel(ctx, rect.x+step, rect.y, col);
    }

    if (ne) { // topright
      mu_draw_pixel(ctx, rect.x+rect.w-1, rect.y+step, col);
      mu_draw_pixel(ctx, rect.x+rect.w-(1+step), rect.y, col);
    }

    if (se) { // bottomright
      mu_draw_pixel(ctx, rect.x+rect.w-1, rect.y+rect.h-(1+step), col);
      mu_draw_pixel(ctx, rect.x+rect.w-(1+step), rect.y+rect.h-1, col);
    }

    if (sw) { // bottomleft
      mu_draw_pixel(ctx, rect.x, rect.y+rect.h-(1+step), col);
      mu_draw_pixel(ctx, rect.x+step, rect.y+rect.h-1, col);
    }
  }

  // color = mu_color(255, 128, 0, 255);
  if (!nw) { // top left
    mu_draw_rect(ctx, mu_rect(rect.x, rect.y, roundness-1, 1), color); // horizontal
    mu_draw_rect(ctx, mu_rect(rect.x, rect.y+1, 1, roundness-2), color); // vertical
  }
  if (!ne) { // top right
    mu_draw_rect(ctx, mu_rect(rect.x+rect.w-roundness+1, rect.y, roundness-1, 1), color); // horizontal
    mu_draw_rect(ctx, mu_rect(rect.x+rect.w-1, rect.y+1, 1, roundness-2), color); // vertical
  }
  if (!se) { // bottom right
    mu_draw_rect(ctx, mu_rect(rect.x+rect.w-roundness+1, rect.y+rect.h-1, roundness-1, 1), color); // horizontal
    mu_draw_rect(ctx, mu_rect(rect.x+rect.w-1, rect.y+rect.h-roundness+1, 1, roundness-2), color); // vertical
  }
  if (!sw) { // bottom left
    mu_draw_rect(ctx, mu_rect(rect.x, rect.y+rect.h-1, roundness-1, 1), color); // horizontal
    mu_draw_rect(ctx, mu_rect(rect.x, rect.y+rect.h-roundness+1, 1, roundness-2), color); // vertical
  }
}

static void mu_draw_rounded_box_margin(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness) {
  mu_draw_rounded_box_margin_ex(ctx, rect, color, roundness, 1, 1, 1, 1);
}

// void mu_draw_rounded_box(mu_Context *ctx, mu_Rect rect, mu_Color color, unsigned int roundness) {
//   expect(roundness > 2 && roundness < 7); // "This only works correctly for roundness from 3px to 6px");

//   if (roundness > 5) {
//     // draw rect 2px inside the frame
//     mu_draw_box(ctx, expand_rect(rect, -2), color);
//     // draw border 1px inside the frame
//     mu_draw_box_inner(ctx, expand_rect(rect, -1), color, 1, mu_max(roundness-5, 0));
//   } else {
//     // draw box 1px inside the frame
//     mu_draw_box(ctx, expand_rect(rect, -1), color);
//   }

//   mu_draw_rounded_box_margin(ctx, rect, color, roundness);
// }

void mu_draw_rounded_rect_ex(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness, uint nw, uint ne, uint se, uint sw) {
  expect(roundness >= 0 && roundness < 7); // "This only works correctly for roundness from 3px to 6px");

  if (roundness == 0) { // 0px
    return mu_draw_rect(ctx, rect, color);
  } else if (roundness < 3) { // 1px or 2px
    // draw rect 1px inside the frame
    mu_draw_rect(ctx, expand_rect(rect, -1), color);
    mu_draw_box_inner(ctx, rect, color, 1, 1); // both have 1px 'blank' corners

    // color = mu_color(255, 128, 0, 255);
    mu_draw_rect_corner_pixels_ex(ctx, rect, color, nw == 0, ne == 0, se == 0, sw == 0);
    return;
  }

  if (roundness > 5) {
    // draw rect 2px inside the frame
    mu_draw_rect(ctx, expand_rect(rect, -2), color);
    // draw border 1px inside the frame
    mu_draw_box_inner(ctx, expand_rect(rect, -1), color, 1, mu_max(roundness-5, 0));

    // cover pixels of non-rounded corners with the original color
    mu_draw_rect_corner_pixels_ex(ctx, expand_rect(rect, -1), color, nw == 0, ne == 0, se == 0, sw == 0);

    // cover pixels of rounded corners with 50% (6px) or 25% (7px) opacity of the original color
    mu_Color col = mu_color(color.r, color.g, color.b, (int)color.a * (1/(roundness - 4.0)));

    mu_draw_rect_corner_pixels_ex(ctx, expand_rect(rect, -1), col, nw, ne, se, sw);

  } else {
    // draw rect 1px inside the frame
    mu_draw_rect(ctx, expand_rect(rect, -1), color);
  }

  mu_draw_rounded_box_margin_ex(ctx, rect, color, roundness, nw, ne, se, sw);
}

void mu_draw_rounded_rect(mu_Context *ctx, mu_Rect rect, mu_Color color, uint roundness) {
  mu_draw_rounded_rect_ex(ctx, rect, color, roundness, 1, 1, 1, 1);
}

void mu_draw_box_shadow(mu_Context *ctx, mu_Rect rect, mu_Color color, float opacity, uint roundness, uint width, int offset_x, int offset_y) {
  if (opacity < 0.05) return;
  mu_Rect r = mu_rect(rect.x-width+offset_x, rect.y-width+offset_y, rect.w+(width*2), rect.h+(width*2));
  color.a *= opacity;
  mu_draw_rounded_rect(ctx, r, color, roundness);
}

void mu_draw_text_scaled(mu_Context *ctx, mu_Font font, const char *str, int len, mu_Vec2 pos, mu_Color color, float scale) {
  mu_Command *cmd;
  mu_Rect rect = mu_rect(
    pos.x, pos.y, ctx->text_width(font, str, len), ctx->font_height(font));
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* add command */
  if (len < 0) { len = strlen(str); }
  cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
  memcpy(cmd->text.str, str, len);
  cmd->text.str[len] = '\0';
  cmd->text.pos = pos;
  cmd->text.color = color;
  cmd->text.font = font;
  cmd->text.scale = scale;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len,
  mu_Vec2 pos, mu_Color color)
{
  mu_draw_text_scaled(ctx, font, str, len, pos, color, 1);
}

void mu_draw_shape(mu_Context *ctx, unsigned char shape, mu_Rect rect, mu_Color color, float scale, float params[]) {
  mu_Command *cmd;
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* do shape command */
  cmd = mu_push_command(ctx, MU_COMMAND_SHAPE, sizeof(mu_ShapeCommand));
  cmd->shape.shape = shape;
  cmd->shape.rect  = rect;
  cmd->shape.color = color;
  cmd->shape.scale = scale;
  cmd->shape.params[0] = params[0];
  cmd->shape.params[1] = params[1];
  cmd->shape.params[2] = params[2];
  cmd->shape.params[3] = params[3];
  cmd->shape.params[4] = params[4];
  cmd->shape.params[5] = params[5];
  cmd->shape.params[6] = params[6];
  cmd->shape.params[7] = params[7];
  cmd->shape.params[8] = params[8];
  cmd->shape.params[9] = params[9];

  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

void mu_draw_icon_scaled(mu_Context *ctx, int id, mu_Rect rect, mu_Color color, float scale) {
  mu_Command *cmd;
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* do icon command */
  cmd = mu_push_command(ctx, MU_COMMAND_ICON, sizeof(mu_IconCommand));
  cmd->icon.id = id;
  cmd->icon.rect = rect;
  cmd->icon.color = color;
  cmd->icon.scale = scale;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  mu_draw_icon_scaled(ctx, id, rect, color, 1);
}

mu_Image mu_new_image(mu_Texture texture, unsigned int width, unsigned int height) {
  return (mu_Image) {
    .texture = texture,
    .width = width,
    .height = height,
    .rect = mu_texture_rect(0.0, 0.0, 1.0, 1.0),
  };
}

mu_Image mu_sub_image(mu_Image image, mu_Rect rect) {
  return (mu_Image) {
    .texture = image.texture,
    .width = rect.w,
    .height = rect.h,
    .rect = {
      (float)rect.x / image.width,
      (float)rect.y / image.height,
      (float)rect.w / image.width,
      (float)rect.h / image.height,
    }
  };
}

void mu_draw_image_ex(mu_Context *ctx, mu_Image image, mu_Rect rect, float opacity, float scale, int flags) {
  mu_Command *cmd;
  /* do clip command if the rect isn't fully contained within the cliprect */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == 0) clipped = 1;
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* do icon command */
  cmd = mu_push_command(ctx, MU_COMMAND_IMAGE, sizeof(mu_ImageCommand));
  cmd->image.img     = image;
  cmd->image.dest    = rect;
  cmd->image.opacity = opacity;
  cmd->image.scale   = scale;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

void mu_draw_image_expand(mu_Context* ctx, mu_Image image, float opacity, float scale, int flags) {
  mu_Rect rect = mu_layout_next(ctx);
  rect.x -= ctx->style->spacing;
  rect.y -= ctx->style->spacing;
  rect.w += ctx->style->spacing * 2;
  rect.h += ctx->style->spacing * 2;

  float scaleW = (rect.w/(float)image.width);
  float scaleH = (rect.h/(float)image.height);
  // printf("scaleW: %f, scaleH: %f\n", scaleW, scaleH);

  if (scaleW > scaleH) {
    rect.y -= (((image.height * scaleW) - rect.h)/2);
    rect.w = image.width * scaleW;
    rect.h = image.height * scaleW;
  } else {
    // if (flags & MU_OPT_ALIGNCENTER)
      rect.x -= (((image.width * scaleH) - rect.w)/2);
    rect.w = image.width * scaleH;
    rect.h = image.height * scaleH;
  }

  mu_draw_image_ex(ctx, image, rect, opacity, scale, flags);
}

void mu_draw_image(mu_Context* ctx, mu_Image image, float opacity, float scale, int flags) {
  mu_Rect rect = mu_layout_next(ctx);

  if (flags & MU_OPT_NOPADDING) {
    rect.x -= ctx->style->padding;
    rect.w += ctx->style->padding * 2;
    rect.y -= ctx->style->padding;
    // rect.h += ctx->style->padding * 2;
  }

  int next_y = 0;
  int next_w = rect.w;

  if (rect.h == ctx->style->size.y + ctx->style->padding * 2) { // layout.h is 0, fluid image requested
    // printf("fluid image requested\n");

    // don't allow image to grow wider than its original width
    if (image.width < rect.w) rect.w = image.width;

    float scaleW = (rect.w/(float)image.width);
    rect.h = image.height * scaleW;
    next_y = rect.y + rect.h; // push or pull next row

  } else if (rect.w >= image.width && rect.h >= image.height) { // rect is larger than image
    // printf("rect is larger than image\n");

    rect.w = image.width;
    rect.h = image.height;

  } else { // rect is smaller than image
    // printf("rect is smaller than image\n");

    float scaleW = (rect.w/(float)image.width);
    float scaleH = (rect.h/(float)image.height);

    int fixed_height = 0;
    if (fixed_height) {
      rect.w = image.width * scaleH;
      rect.h = image.height * scaleH;
    } else { // shrink if rect height is less than image height
      // printf("scaleW vs scaleH: %f / %f\n", scaleW, scaleH);
      if (scaleW > scaleH) {
        rect.w = image.width * scaleH;
        rect.h = image.height * scaleH;
      } else {
        rect.h = image.height * scaleW;
        next_y = rect.y + rect.h; // pull next row up
      }
    }
  }

  mu_Layout *layout = get_layout(ctx);
  mu_Container *cnt = mu_get_current_container(ctx);

  if (flags & MU_OPT_CONTAINHEIGHT) {
    int max_height = (cnt->body.h - layout->position.y) - ctx->style->spacing;
    if (max_height < rect.h) {
      float scaleH = (max_height/(float)rect.h);
      rect.h = max_height;
      rect.w *= scaleH;
    }
  }

  if (flags & MU_OPT_ALIGNCENTER) {
    // position at center of container
    rect.x = (cnt->body.w - rect.w) / 2;
    rect.y = (cnt->body.h - rect.h) / 2;
  }

  mu_draw_image_ex(ctx, image, rect, opacity, scale, flags);

  if (next_y != 0) { // modify next rect
    mu_layout_set_next(ctx, mu_rect(rect.x, next_y, next_w, rect.h), 0);
  }
}

/*============================================================================
** layout
**============================================================================*/

enum { RELATIVE = 1, ABSOLUTE = 2 };


void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}


void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* inherit position/next_row/max from child layout if they are greater */
  a = get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}


void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height) {
  mu_Layout *layout = get_layout(ctx);
  if (widths) {
    expect(items <= MU_MAX_WIDTHS);
    memcpy(layout->widths, widths, items * sizeof(widths[0]));
  }
  layout->items = items;
  layout->position = mu_vec2(layout->indent, layout->next_row);
  layout->size.y = height;
  layout->item_index = 0;
}


void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}


void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}


void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}


mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if (layout->next_type) {
    /* handle rect set by `mu_layout_set_next` */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }

  } else {
    /* handle next row */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    /* position */
    res.x = layout->position.x;
    res.y = layout->position.y;

    /* size */
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* update position */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* apply body offset */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* update max position */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}


/*============================================================================
** controls
**============================================================================*/

static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.idx;
  while (i--) {
    if (ctx->container_stack.items[i] == ctx->hover_root) { return 1; }
    /* only root containers have their `head` field set; stop searching if we've
    ** reached the current root container */
    if (ctx->container_stack.items[i]->head) { break; }
  }
  return 0;
}


void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}

void mu_draw_control_text_ex(mu_Context *ctx, const char *str, mu_Rect rect,
  mu_Color color, int opt, int offset_x, int offset_y, float scale)
{
  mu_Vec2 pos;
  mu_Font font = ctx->style->font;
  int tw = ctx->text_width(font, str, -1) * scale;
  mu_push_clip_rect(ctx, rect);

  pos.y = rect.y + (rect.h - ctx->font_height(font) * scale) / 2;

  if (opt & MU_OPT_ALIGNCENTER) {
    pos.x = rect.x + (rect.w - tw) / 2;
  } else if (opt & MU_OPT_ALIGNRIGHT) {
    pos.x = rect.x + rect.w - tw - ctx->style->padding;
  } else {
    pos.x = rect.x + ctx->style->padding;
  }

  if (opt & MU_OPT_ALIGNTOP) {
    pos.y = rect.y + ctx->style->padding;
  }
  if (opt & MU_OPT_ALIGNBOTTOM) {
    pos.y = rect.y + rect.h - ctx->font_height(font) - ctx->style->padding;
  }

  pos.x += offset_x;
  pos.y += offset_y;

  mu_draw_text_scaled(ctx, font, str, -1, pos, color, scale);
  mu_pop_clip_rect(ctx);
}

void mu_draw_control_text_scaled(mu_Context *ctx, const char *str, mu_Rect rect,
  int colorid, int opt, int offset_x, int offset_y, float scale)
{
  mu_draw_control_text_ex(ctx, str, rect, ctx->style->colors[colorid], opt, offset_x, offset_y, scale);
}

void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect,
  int colorid, int opt, int offset_x, int offset_y)
{
  mu_draw_control_text_ex(ctx, str, rect, ctx->style->colors[colorid], opt, offset_x, offset_y, 1);
}

int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) &&
    rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
    in_hover_root(ctx);
}


void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt) {
  int mouseover = mu_mouse_over(ctx, rect);

  if (ctx->focus == id) { ctx->updated_focus = 1; }
  if (opt & MU_OPT_NOINTERACT) { return; }

  // if (mouseover) {
  //   printf("ctx->mouse_down: %d, ctx->mouse_pressed %d\n", ctx->mouse_down, ctx->mouse_pressed);
  // }

  // if (mouseover && !ctx->mouse_down) {
  if (mouseover) {
    ctx->hover = id;
    ctx->prevent_scroll = opt & MU_OPT_PREVENT_SCROLL ? 1 : 0;
  }

  if (ctx->focus == id) {
    if (ctx->mouse_pressed && !mouseover) { mu_set_focus(ctx, 0); }
    if (!ctx->mouse_down && ~opt & MU_OPT_HOLDFOCUS) { mu_set_focus(ctx, 0); }
  }

  if (ctx->hover == id) {
    if (ctx->mouse_pressed || ctx->mouse_up) {
      mu_set_focus(ctx, id);
    } else if (!mouseover) {
      ctx->hover = 0;
      ctx->hover_type = 0;
    }
  }
}

void mu_text_with_color(mu_Context *ctx, const char *text, mu_Font font, mu_Color color, int xoff, int yoff) {
  const char *start, *end, *p = text;
  int width = -1;
  mu_layout_begin_column(ctx);
  mu_layout_row(ctx, 1, &width, ctx->line_height(font));

  do {
    mu_Rect r = mu_layout_next(ctx);
    int w = 0;
    start = end = p;
    do {
      const char* word = p;
      while (*p && *p != ' ' && *p != '\n') { p++; }
      w += ctx->text_width(font, word, p - word);
      if (w > r.w && end != start) { break; }
      w += ctx->text_width(font, p, 1);
      end = p++;
    } while (*end && *end != '\n');
    mu_draw_text(ctx, font, start, end - start, mu_vec2(r.x + xoff, r.y + yoff), color);
    p = end + 1;
  } while (*end);

  mu_layout_end_column(ctx);
}

void mu_text_ex(mu_Context *ctx, const char *text, mu_Font font) {
  mu_text_with_color(ctx, text, font, ctx->style->colors[MU_COLOR_TEXT], 0, 0);
}

void mu_text(mu_Context *ctx, const char *text) {
  mu_text_ex(ctx, text, ctx->style->font);
}

void mu_text_with_shadow_ex(mu_Context *ctx, const char *text, unsigned char xoff, unsigned char yoff, unsigned char shadow_opacity, mu_Font font) {
  mu_text_with_color(ctx, text, font, mu_color(0,0,0,shadow_opacity), xoff, yoff);
  mu_text_with_color(ctx, text, font, ctx->style->colors[MU_COLOR_TEXT], 0, 0);
}

void mu_text_with_shadow(mu_Context *ctx, const char *text, unsigned char xoff, unsigned char yoff, unsigned char shadow_opacity) {
  mu_text_with_shadow_ex(ctx, text, xoff, yoff, shadow_opacity, ctx->style->font);
}

void mu_label(mu_Context *ctx, const char *text, int opt) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, opt, 0, 0);
}

void mu_label_ex(mu_Context *ctx, const char *text, mu_Color color, unsigned char xoff, unsigned char yoff, float scale, int opt) {
  mu_Rect r = mu_layout_next(ctx);
  mu_draw_control_text_ex(ctx, text, r, color, opt, xoff, yoff, scale);
}

void mu_label_with_shadow(mu_Context *ctx, const char *text, unsigned char xoff, unsigned char yoff, unsigned char shadow_opacity, int opt) {
  mu_Rect r = mu_layout_next(ctx);
  if (shadow_opacity > 0) mu_draw_control_text_ex(ctx, text, r, mu_color(0, 0, 0, shadow_opacity), opt, xoff, yoff, 1);
  mu_draw_control_text(ctx, text, r, MU_COLOR_TEXT, opt, 0, 0);
}

int mu_button_ex_with_shadow(mu_Context *ctx, const char *label, int icon, int opt, unsigned char xoff, unsigned char yoff, unsigned char shadow_opacity) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label)) : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  if (ctx->hover == id) ctx->hover_type = 1; // button

  mu_Color text_color = ctx->style->colors[MU_COLOR_TEXT];

  if (opt & MU_OPT_NOINTERACT) {
    // text_color.a = 128;
    text_color.a = 150;
  } else {
    /* handle click */
    // if ((opt & MU_OPT_TRIGGER_ON_MOUSE_DOWN && (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id)) || (ctx->mouse_up == MU_MOUSE_LEFT && ctx->focus == id)) {
    // if (ctx->mouse_up == MU_MOUSE_LEFT && ctx->focus == id) {
    if (ctx->mouse_up && ctx->focus == id) {
      res |= MU_RES_SUBMIT;
    }
  }

  /* draw */
  if (~opt & MU_OPT_BURIED || (ctx->hover == id || ctx->focus == id)) {
    int colorid = MU_COLOR_BUTTON;
    colorid += (ctx->focus == id) ? 2 : (ctx->hover == id ? 1 : 0);
#ifdef USE_SHAPES_FOR_BUTTONS
    // border_radius_top
    // border_radius_bottom
    // shadow_opacity
    // shadow_offset_x
    // shadow_offset_y
    // shadow_blur
    // shadow_spread
    // bg_blur_amount
    // border_opacity

    float params[9] = {4, 4, 0.3, 0, 4, 4, -4, 0, 0};
    // we need to expand rect by 1 px, to match mu_draw_control_frame's behaviour
    // that calls mu_draw_rect and then mu_draw_box_outer
    mu_draw_shape(ctx, MU_SHAPE_RECT, expand_rect(r, 1), ctx->style->colors[colorid], 1, params);
#else
    mu_draw_control_frame(ctx, id, r, colorid, opt);
#endif
  }

  if (label) {
    int label_yoff = opt & MU_OPT_RAISED && ctx->focus != id ? -1 : 0;
    if (shadow_opacity > 0 && (xoff != 0 || yoff != 0)) { // draw text shadow
      mu_draw_control_text_ex(ctx, label, r, mu_color(0, 0, 0, shadow_opacity), opt, xoff, label_yoff + yoff, 1);
    }
    mu_draw_control_text_ex(ctx, label, r, text_color, opt, 0, label_yoff, 1);
  }

  if (icon) { mu_draw_icon(ctx, icon, r, text_color); }
  return res;
}

int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  return mu_button_ex_with_shadow(ctx, label, icon, opt, 0, 0, 0);
}

int mu_color_button_ex(mu_Context *ctx, const char *label, mu_Color color, int opt) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);
  if (ctx->hover == id) ctx->hover_type = 1; // button

  // int highlight = (
  //                0.2126*((float)((color >> 24) & 0xFF)/255) +
  //                0.7152*((float)((color >> 16) & 0xFF)/255) +
  //                0.0722*((float)((color >>  8) & 0xFF)/255)
  //            ) ? 0: 255;

  int highlight = (0.2126*((float)(color.r)/255) +
                   0.7152*((float)(color.g)/255) +
                   0.0722*((float)(color.b)/255)) ? 0 : 255;

  // if (!(opt & MU_OPT_NOFRAME)) ctx->draw_frame(ctx, r, color);
  if (!(opt & MU_OPT_NOFRAME)) ctx->draw_frame(ctx, r, 0);

  // if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
  if (ctx->mouse_up == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_SUBMIT;
  }

  mu_draw_rect(ctx, r, mu_color(highlight, highlight, highlight,
    ctx->focus == id ? 100 : ctx->hover == id ? 50 : 0
  ));

  mu_Vec2 pos;
  mu_Font font = ctx->style->font;
  int tw = ctx->text_width(font, label, -1);
  mu_push_clip_rect(ctx, r);
  pos.y = r.y + (r.h - ctx->font_height(font)) / 2;

  if (opt & MU_OPT_ALIGNCENTER) {
    pos.x = r.x + (r.w - tw) / 2;
  } else if (opt & MU_OPT_ALIGNRIGHT) {
    pos.x = r.x + r.w - tw - ctx->style->padding;
  } else {
    pos.x = r.x + ctx->style->padding;
  }

  mu_draw_text(ctx, font, label, -1, pos, mu_color(highlight, highlight, highlight, 255));
  mu_pop_clip_rect(ctx);

  return res;
}

int mu_checkbox(mu_Context *ctx, const char *label, int *state) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, &state, sizeof(state));
  mu_Rect r = mu_layout_next(ctx);
  mu_Rect box = mu_rect(r.x, r.y, r.h, r.h);
  mu_update_control(ctx, id, r, 0);
  if (ctx->hover == id) ctx->hover_type = 1; // button

  /* handle click */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_CHANGE;
    *state = !*state;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, box, MU_COLOR_BASE, 0);
  if (*state) {
    mu_draw_icon(ctx, MU_ICON_CHECK, box, ctx->style->colors[MU_COLOR_TEXT]);
  }
  r = mu_rect(r.x + box.w, r.y, r.w - box.w, r.h);
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0, 0, 0);
  return res;
}

static inline int find_last_char_in_str(char *str, unsigned int len, unsigned int current_pos, char ch) {
  int i, c, pos = current_pos-1;
  c = pos;

  // fix offset pos accounting for utf-8 continuation chars
  for (i = 0; i < pos; i++) {
    if ((str[i] & 0xc0) == 0x80) { pos++; }
  }

  while (pos > 0 && str[pos] == ch) { pos--; c--; }

  if (pos == 0) return -1;

  for (i = pos; i > 0; i--) {
    if ((str[i] & 0xc0) == 0x80) continue;
    else if (str[i] == ch) return c+1;
    c--;
  }

  return -1;
}

static inline int find_next_char_in_str(char *str, unsigned int len, unsigned int current_pos, char ch) {
  int i, c, pos = current_pos+1;
  c = pos;

  // fix offset pos accounting for utf-8 continuation chars
  for (i = 0; i < pos; i++) {
    if ((str[i] & 0xc0) == 0x80) { pos++; }
  }

  while (pos <= len && str[pos] == ch) { pos++; c++; }

  if (pos == len) return -1;

  // printf("current_pos: %d, starting pos: %d, char is %c\n", current_pos, pos, str[pos]);
  for (i = pos; i < len; i++) {
    if ((str[i] & 0xc0) == 0x80) continue;
    else if (str[i] == ch) return c;
    c++;
  }

  return -1;
}

static int find_textbox_index_at_position(mu_Context *ctx, mu_Font font, char *buf, unsigned int len, int pos) {
  if (pos <= 0) return 0;

  int w, i;
  int start = (int)(pos / 14) + 1; // no need to start at position 0
  // printf("starting at char %d\n", start);
  if (start > len) return len;

  int lastw;
  for (i = start; i <= len; i++) {
    w = ctx->text_width(font, buf, i-1);
    // printf("width at %d is %d (pos is %d)\n", i, w, pos);
    if (w > pos) { // check if this position is closer or the last one
      // printf("prev diff: %d, this diff: %d\n", pos-lastw, w-pos);
      return pos-lastw < w-pos ? i-2 : i-1;
    }
    lastw = w;
  }

  return len;
}

static void str_delete_fwd(char * str, int len, int index, int chars_to_remove) {
  if (index == len || len == 0) return;

  int bytes_to_remove = chars_to_remove;

  // count hoy many actual bytes we need to remove
  int c = chars_to_remove, i = index;
  while (c--) {
    while ((str[++i] & 0xc0) == 0x80) { bytes_to_remove++; }
    if (i >= len) break;
  }

  // printf("removing %d chars (%d bytes), index %d, len %d\n", chars_to_remove, bytes_to_remove, index, len);
  memmove(str + index, str + index + bytes_to_remove, len - index - bytes_to_remove);
  str[len - bytes_to_remove] = '\0';
}

static void str_delete_back(char * str, int len, int index, int chars_to_remove) {
  if (index == 0 || len == 0) return;

  int bytes_to_remove = chars_to_remove;

  int c = chars_to_remove, i = index;
  while (c--) {
    while ((str[--i] & 0xc0) == 0x80) { bytes_to_remove++; }
    if (i <= 0) break;
  }

  index -= (bytes_to_remove);
  memmove(str + index, str + index + bytes_to_remove, len - index - bytes_to_remove);
  str[len - bytes_to_remove] = '\0';
}

int mu_delete_selection(mu_Context * ctx, char * buf, int bytelen) {
  int min_byte_index = num_bytes_until(buf, bytelen, 0, ctx->textbox_select_min);
  int chars_to_delete = ctx->textbox_select_max - ctx->textbox_select_min;

  str_delete_fwd(buf, bytelen, min_byte_index, chars_to_delete);

  if (ctx->textbox_index == ctx->textbox_select_max) ctx->textbox_index -= chars_to_delete;
  ctx->textbox_select_min = ctx->textbox_select_max = 0;
  return chars_to_delete;
}

int mu_has_selection(mu_Context * ctx) {
  return ctx->textbox_select_max > 0 && ctx->textbox_select_min != ctx->textbox_select_max;
}

int mu_select_back(mu_Context * ctx, int index) {
  if (mu_has_selection(ctx)) {
    if (index == 0 || index < ctx->textbox_select_min) {
      ctx->textbox_select_min = index;
    } else {
      ctx->textbox_select_max = index;
    }
  } else {
    ctx->textbox_select_min = index;
    ctx->textbox_select_max = ctx->textbox_index;
  }
}

int mu_select_forward(mu_Context * ctx, int index, int charlen) {
  if (mu_has_selection(ctx)) {
    if (index == charlen || index > ctx->textbox_select_max) {
      ctx->textbox_select_max = index;
    } else {
      ctx->textbox_select_min = index;
    }
  } else {
    ctx->textbox_select_min = ctx->textbox_index;
    ctx->textbox_select_max = index;
  }
}

int mu_select_word_at_pos(mu_Context * ctx, char * buf, int pos, int charlen) {
  int word_start = find_last_char_in_str(buf, charlen, pos, ' ');
  int word_end   = find_next_char_in_str(buf, charlen, pos, ' ');
  ctx->textbox_select_min = word_start == -1 ? 0 : word_start;
  ctx->textbox_select_max = word_end == -1 ? charlen : word_end;
}

int mu_clear_selection(mu_Context * ctx) {
  ctx->textbox_select_min = ctx->textbox_select_max = 0;
}

static void set_copy_text_from_selection(mu_Context *ctx, char *buf, int bytelen) {

  int byte_start = num_bytes_until(buf, bytelen, 0, ctx->textbox_select_min);
  int num_bytes = num_bytes_until(buf, bytelen, byte_start, ctx->textbox_select_max);
  if (num_bytes == 0) return;

  // strncpy(copy_text, buf, num_bytes);
  int i, c = 0;
  for (i = byte_start; i < (byte_start + num_bytes); i++) {
    ctx->copy_text[c++] = buf[i];
  }

  ctx->copy_text[c] = '\0';
}

int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r, int opt) {
  int res = 0;

  mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);
  if (ctx->hover == id) ctx->hover_type = 2; // textbox

  if (ctx->focus == id) {

    /* handle text input */
    int bytelen = strlen(buf);
    int charlen = bytelen == 0 ? 0 : num_chars(buf);
    int byte_index = bytelen == 0 ? 0 : num_bytes_until(buf, bytelen, 0, ctx->textbox_index);
    int n = mu_min(bufsz - bytelen - 1, num_bytes(ctx->input_text));

    int current_index = ctx->textbox_index;
    int has_selection = ctx->textbox_select_max > 0 && ctx->textbox_select_min != ctx->textbox_select_max;

    if (n > 0) { // new text!

      if (ctx->key_down & MU_KEY_CTRL) {

        if (ctx->input_text[0] == 'a') { // Ctrl + A

          ctx->textbox_select_min = 0;
          ctx->textbox_select_max = charlen;
          ctx->textbox_index = charlen;
          current_index = -1;

        } else if (ctx->input_text[0] == 'x') { // Ctrl + X

          if (has_selection) {
            set_copy_text_from_selection(ctx, buf, bytelen);
            mu_delete_selection(ctx, buf, bytelen);
          } else { // cut whole line
            // TODO
          }

        } else if (ctx->input_text[0] == 'c') { // Ctrl + V
          if (has_selection) set_copy_text_from_selection(ctx, buf, bytelen);
        }

      } else { // Ctrl not down, so insert input

        if (has_selection) {
          int deleted_chars = mu_delete_selection(ctx, buf, bytelen);
          bytelen = strlen(buf);
          byte_index = num_bytes_until(buf, bytelen, 0, ctx->textbox_index);
        }

        int i = byte_index;
        int target_pos = bytelen - i;

        if (target_pos > 0) memmove(buf + i + n, buf + i, target_pos);
        memcpy(buf + i, ctx->input_text, n);
        bytelen += n;
        buf[bytelen] = '\0';
        charlen = num_chars(buf);

        ctx->textbox_index += num_chars(ctx->input_text);
        res |= MU_RES_CHANGE;
      }
    }

    if (charlen > 0) {

      if (ctx->mouse_pressed == MU_MOUSE_LEFT || ctx->mouse_down == MU_MOUSE_LEFT) {

        mu_Font font = ctx->style->font;
        int pos = ctx->mouse_pos.x - r.x - ctx->style->padding;
        int idx = find_textbox_index_at_position(ctx, font, buf, charlen, pos);

        if (ctx->mouse_pressed) {

          if (ctx->last_click_time < 0.2) { // double click
            if (idx > -1) {
              mu_select_word_at_pos(ctx, buf, idx, charlen);
              ctx->textbox_index = ctx->textbox_select_max;
              current_index = -1; // so we skip the 'if moved cursor' check'
            }
          } else if (ctx->key_down & MU_KEY_SHIFT) {
            if (idx > ctx->textbox_index) {
              mu_select_forward(ctx, idx == -1 ? charlen : idx, charlen);
            } else if (idx < ctx->textbox_index) {
              mu_select_back(ctx, idx);
            }
          } else {
            ctx->textbox_select_min = idx;
            ctx->textbox_select_max = 0;
          }

          ctx->textbox_index = idx;
          current_index = -1; // so we skip the 'if moved cursor' check'

        } else if (idx != ctx->textbox_index && ctx->mouse_delta.x != 0) {
          if (ctx->mouse_delta.x > 0) { // moving forward
            mu_select_forward(ctx, idx == -1 ? charlen : idx, charlen);
          } else {
            mu_select_back(ctx, idx);
          }

          ctx->textbox_index = idx;
          current_index = -1; // so we skip the 'if moved cursor' check'
        }
      }

      /* backspace */
      if (ctx->key_pressed & MU_KEY_BACKSPACE && ctx->textbox_index > 0) {
        if (has_selection) {
          mu_delete_selection(ctx, buf, bytelen);
          res |= MU_RES_CHANGE;

        } else {
          int chars_to_delete = 1;

          if ((ctx->key_down & MU_KEY_CTRL) || (ctx->key_down & MU_KEY_ALT)) {
            int idx = find_last_char_in_str(buf, charlen, ctx->textbox_index, ' ');
            if (idx == -1) idx = 0;
            chars_to_delete = ctx->textbox_index - idx;
          }

          str_delete_back(buf, bytelen, byte_index, chars_to_delete);
          ctx->textbox_index -= chars_to_delete;
          res |= MU_RES_CHANGE;
        }
      }

      /* delete */
      if (ctx->key_pressed & MU_KEY_DELETE) {
        if (bytelen == 0 || (byte_index-1) == (bytelen+1)) {
          // nothing to delete
        } else {
          if (has_selection) {
            mu_delete_selection(ctx, buf, bytelen);
            res |= MU_RES_CHANGE;

          } else if (ctx->textbox_index < charlen) {
            int chars_to_delete = 1;
            if ((ctx->key_down & MU_KEY_CTRL)) {
              int idx = find_next_char_in_str(buf, charlen, ctx->textbox_index, ' ');
              if (idx == -1) idx = charlen;
              chars_to_delete = idx - ctx->textbox_index;
            }

            str_delete_fwd(buf, bytelen, byte_index, chars_to_delete);
            res |= MU_RES_CHANGE;
          }
        }
      }

      /* return */
      if (ctx->key_pressed & MU_KEY_RETURN) {
        mu_set_focus(ctx, 0);
        res |= MU_RES_SUBMIT;
      }

      if (ctx->key_down & MU_KEY_HOME) {
        if (ctx->key_down & MU_KEY_SHIFT) {
          mu_select_back(ctx, 0);
          current_index = -1;
        } else {
          mu_clear_selection(ctx);
        }
        ctx->textbox_index = 0;
      } else if (ctx->key_down & MU_KEY_END) {
        if (ctx->key_down & MU_KEY_SHIFT) {
          mu_select_forward(ctx, charlen, charlen);
          current_index = -1;
        } else {
          mu_clear_selection(ctx);
        }
        ctx->textbox_index = charlen;
      }

      /* left and right */
      if (ctx->key_pressed & MU_KEY_LEFT) {

        if (ctx->textbox_index > 0) {
          if (ctx->key_down & MU_KEY_CTRL) {
            int idx = find_last_char_in_str(buf, charlen, ctx->textbox_index, ' ');

            if (ctx->key_down & MU_KEY_SHIFT) {
              mu_select_back(ctx, idx == -1 ? 0 : idx);
              current_index = -1; // so we skip the 'if moved cursor' check'
            }

            ctx->textbox_index = idx == -1 ? 0 : idx;

          } else {
            if (ctx->key_down & MU_KEY_SHIFT) {
              mu_select_back(ctx, ctx->textbox_index-1);
              current_index = -1; // so we skip the 'if moved cursor' check'
            }

            ctx->textbox_index--;
          }
        } else if (!(ctx->key_down & MU_KEY_SHIFT)) { // unselect
          mu_clear_selection(ctx);
        }

      } else if (ctx->key_pressed & MU_KEY_RIGHT) {

        if (ctx->textbox_index < charlen) {
          if (ctx->key_down & MU_KEY_CTRL) {
            int idx = find_next_char_in_str(buf, charlen, ctx->textbox_index, ' ');

            if (ctx->key_down & MU_KEY_SHIFT) {
              mu_select_forward(ctx, idx == -1 ? charlen : idx, charlen);
              current_index = -1; // so we skip the 'if moved cursor' check'
            }

            ctx->textbox_index = idx == -1 ? charlen : idx;

          } else {
            if (ctx->key_down & MU_KEY_SHIFT) {
              mu_select_forward(ctx, ctx->textbox_index+1, charlen);
              current_index = -1; // so we skip the 'if moved cursor' check'
            }

            ctx->textbox_index++;
          }
        } else if (!(ctx->key_down & MU_KEY_SHIFT)) { // unselect
          mu_clear_selection(ctx);
        }
      }

      if ((ctx->textbox_index) < 0) ctx->textbox_index = 0;
      if (ctx->textbox_index >= charlen) ctx->textbox_index = charlen;

      // if user moved cursor, then reset selection
      if (current_index > -1 && ctx->textbox_index != current_index) {
        mu_clear_selection(ctx);
      }
    }

  } // if charlen > 0

  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BASE, opt);
  if (ctx->focus == id) {
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
    mu_Font font = ctx->style->font;
    int textw = ctx->text_width(font, buf, ctx->textbox_index);
    int texth = ctx->font_height(font);
    int ofx = r.w - ctx->style->padding - textw - 1;
    int textx = r.x + mu_min(ofx, ctx->style->padding);
    int texty = r.y + (r.h - texth) / 2;
    mu_push_clip_rect(ctx, r);

    // draw selection
    if (ctx->textbox_select_max > 0 && ctx->textbox_select_min != ctx->textbox_select_max) {
      int width_min = ctx->text_width(font, buf, ctx->textbox_select_min);
      int width_max = ctx->text_width(font, buf, ctx->textbox_select_max);

      ofx = r.x + ctx->style->padding;
      int ymargin = 6;

      mu_Rect selection = mu_rect((ofx + width_min)-1, r.y+ymargin, (width_max - width_min) + 1, r.h-ymargin*2);
      mu_draw_rect(ctx, selection, mu_color(70, 150, 220, 128));
    }

    mu_draw_text(ctx, font, buf, -1, mu_vec2(textx, texty), color);

    // cursor flicker
    color.a = ctx->frame % 64 > 32 ? 255 : 0;

#ifdef ASEPRITE_FONT
    mu_draw_rect(ctx, mu_rect(textx + textw - 1, texty + 4, 2, texth - 8), color);
#else
    mu_draw_rect(ctx, mu_rect(textx + textw, texty, 1, texth), color);
#endif

    mu_pop_clip_rect(ctx);
  } else {
    mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, opt, 0, 0);
  }

  return res;
}

static int number_textbox(mu_Context *ctx, mu_Real *value, mu_Rect r, mu_Id id) {
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->key_down & MU_KEY_SHIFT && ctx->hover == id ) {
    ctx->number_edit = id;
    sprintf(ctx->number_edit_buf, MU_REAL_FMT, *value);
  }
  if (ctx->number_edit == id) {
    int res = mu_textbox_raw(
      ctx, ctx->number_edit_buf, sizeof(ctx->number_edit_buf), id, r, 0);
    if (res & MU_RES_SUBMIT || ctx->focus != id) {
      *value = strtod(ctx->number_edit_buf, NULL);
      ctx->number_edit = 0;
    } else {
      return 1;
    }
  }
  return 0;
}


int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}


int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high,
  mu_Real step, const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  mu_Rect thumb;
  int x, w, res = 0;
  mu_Real last = *value, v = last;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);

  base.y += ctx->style->padding/2;
  base.h -= ctx->style->padding/2;

  // base.w -= ctx->style->padding/2;

  /* handle text input mode */
  if (number_textbox(ctx, &v, base, id)) { return res; }

  /* handle normal mode */
  // mu_update_control(ctx, id, base, opt);
  mu_update_control(ctx, id, base, opt | MU_OPT_PREVENT_SCROLL);
  if (ctx->hover == id) ctx->hover_type = 3; // slider

  /* handle input */
  if (ctx->focus == id && (ctx->mouse_down | ctx->mouse_pressed) == MU_MOUSE_LEFT) {
    v = low + (ctx->mouse_pos.x - base.x) * (high - low) / base.w;
    // if (step) { v = (((v + step / 2) / step)) * step; }
    if (step) v = (int)(v / step + (v < 0 ? -0.5 : 0.5)) * step;
  }

  if (ctx->hover == id && ctx->scroll_delta.y) {
    v += (ctx->scroll_delta.y / 10);
    if (step) { v = (((v + step / 2) / step)) * step; }
  }

  /* clamp and store value, update res */
  *value = v = mu_clamp(v, low, high);
  if (last != v) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);

  /* draw thumb */
  w = ctx->style->thumb_size;
  x = (v - low) * (base.w - w) / (high - low);
  thumb = mu_rect(base.x + x, base.y, w, base.h);
  mu_draw_control_frame(ctx, id, thumb, MU_COLOR_HANDLE, opt);

  /* draw text  */
  sprintf(buf, fmt, v);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt, 0, 0);

  return res;
}


int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step,
  const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  int res = 0;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);
  mu_Real last = *value;

  /* handle text input mode */
  if (number_textbox(ctx, value, base, id)) { return res; }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    *value += ctx->mouse_delta.x * step;
  }
  /* set flag if value changed */
  if (*value != last) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw text  */
  sprintf(buf, fmt, *value);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt, 0, 0);

  return res;
}


static int header(mu_Context *ctx, const char *label, int istreenode, int opt) {
  mu_Rect r;
  int active, expanded;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  int width = -1;
  mu_layout_row(ctx, 1, &width, 0);

  active = (idx >= 0);
  expanded = (opt & MU_OPT_EXPANDED) ? !active : active;
  r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);

  /* handle click */
  active ^= (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);

  /* update pool ref */
  if (idx >= 0) {
    if (active) { mu_pool_update(ctx, ctx->treenode_pool, idx); }
           else { memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem)); }
  } else if (active) {
    mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  }

  /* draw */
  if (istreenode) {
    if (ctx->hover == id) { ctx->draw_frame(ctx, r, MU_COLOR_BUTTONHOVER); }
  } else {
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, 0);
  }

  mu_draw_icon(
    ctx, expanded ? MU_ICON_EXPANDED : MU_ICON_COLLAPSED,
    mu_rect(r.x, r.y, r.h, r.h), ctx->style->colors[MU_COLOR_TEXT]);

  r.x += r.h - ctx->style->padding;
  r.w -= r.h - ctx->style->padding;

  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0, 0, opt & MU_OPT_RAISED && ctx->focus != id ? -1 : 0);

  return expanded ? MU_RES_ACTIVE : 0;
}


int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, 0, opt);
}


int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if (res & MU_RES_ACTIVE) {
    get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->last_id);
  }
  return res;
}


void mu_end_treenode(mu_Context *ctx) {
  get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}


#define scrollbar(ctx, cnt, b, cs, x, y, w, h)                              \
  do {                                                                      \
    /* only add scrollbar if content size is larger than body */            \
    int maxscroll = cs.y - b->h;                                            \
                                                                            \
    if (maxscroll > 0 && b->h > 0) {                                        \
      mu_Rect base, thumb;                                                  \
      mu_Id id = mu_get_id(ctx, "!scrollbar" #y, 11);                       \
                                                                            \
      /* get sizing / positioning */                                        \
      base = *b;                                                            \
      base.x = b->x + b->w;                                                 \
      base.w = ctx->style->scrollbar_size;                                  \
      int colorid = MU_COLOR_SCROLLTHUMB;                                   \
                                                                            \
      /* handle input */                                                    \
      mu_update_control(ctx, id, base, 0);                                  \
                                                                            \
      if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {           \
        cnt->scroll.y += ctx->mouse_delta.y * cs.y / base.h;                \
        colorid += 1;                                                       \
      } else if (ctx->hover == id) colorid += 1;                            \
                                                                            \
      /* clamp scroll to limits */                                          \
      cnt->scroll.y = mu_clamp(cnt->scroll.y, 0, maxscroll);                \
                                                                            \
      /* draw base and thumb */                                             \
      ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);                      \
      thumb = base;                                                         \
      thumb.h = mu_max(ctx->style->thumb_size, base.h * b->h / cs.y);       \
      thumb.y += cnt->scroll.y * (base.h - thumb.h) / maxscroll;            \
      ctx->draw_frame(ctx, thumb, colorid);                                 \
                                                                            \
      /* set this as the scroll_target (will get scrolled on mousewheel) */ \
      /* if the mouse is over it */                                         \
      if (mu_mouse_over(ctx, *b)) { ctx->scroll_target = cnt; }             \
    } else {                                                                \
      cnt->scroll.y = 0;                                                    \
    }                                                                       \
  } while (0)


static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding * 2;
  cs.y += ctx->style->padding * 2;
  mu_push_clip_rect(ctx, *body);
  /* resize body to make room for scrollbars */
  if (cs.y > cnt->body.h) { body->w -= sz + 2; }
  if (cs.x > cnt->body.w) { body->h -= sz + 2; }
  // if (cs.y > cnt->body.h) { body->w -= sz + 2; }
  // if (cs.x > cnt->body.w) { body->h -= sz + 2; }
  /* to create a horizontal or vertical scrollbar almost-identical code is
  ** used; only the references to `x|y` `w|h` need to be switched */
  // body->x -= 2;
  body->y += 2;
  body->h -= 4;
  scrollbar(ctx, cnt, body, cs, x, y, w, h); // vertical

  body->x += 2;
  body->w -= 4;
  body->y -= 2;
  body->h += 4;

  scrollbar(ctx, cnt, body, cs, y, x, h, w); // horizontal
  mu_pop_clip_rect(ctx);
}


static void push_container_body(
  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt
) {
  if (~opt & MU_OPT_NOSCROLL) { scrollbars(ctx, cnt, &body); }
  push_layout(ctx, expand_rect(body, -ctx->style->padding), cnt->scroll);
  cnt->body = body;
}


static void begin_root_container(mu_Context *ctx, mu_Container *cnt, int opt) {
  push(ctx->container_stack, cnt);
  /* push container to roots list and push head command */
  push(ctx->root_list, cnt);
  cnt->head = push_jump(ctx, NULL);
  /* set as hover root if the mouse is overlapping this container and it has a
  ** higher zindex than the current hover root */
  if (~opt & MU_OPT_NOINTERACT && rect_overlaps_vec2(cnt->rect, ctx->mouse_pos) &&
      (!ctx->next_hover_root || cnt->zindex > ctx->next_hover_root->zindex)
  ) {
    ctx->next_hover_root = cnt;
  }
  /* clipping is reset here in case a root-container is made within
  ** another root-containers's begin/end block; this prevents the inner
  ** root-container being clipped to the outer */
  push(ctx->clip_stack, unclipped_rect);
}


static void end_root_container(mu_Context *ctx) {
  /* push tail 'goto' jump command and set head 'skip' command. the final steps
  ** on initing these are done in mu_end() */
  mu_Container *cnt = mu_get_current_container(ctx);
  cnt->tail = push_jump(ctx, NULL);
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;
  /* pop base clip rect and container */
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}

void mu_set_window_rect(mu_Context *ctx, const char *title, mu_Rect rect) {
  mu_Id id = mu_get_id(ctx, title, strlen(title));
  mu_Container *cnt = get_container(ctx, id, 0);
  cnt->rect = rect;
}

static void draw_resize_handle(mu_Context *ctx, mu_Rect rect) {
  // mu_Color color = mu_color(0, 0, 0, 150);
  mu_Color color = ctx->style->colors[MU_COLOR_BORDER];
  // int bottomx = rect.x + rect.w - 1;
  // int bottomy = rect.y + rect.h - 1;

  char icon_w = 7;
  char icon_h = 7;

  mu_draw_icon(ctx, MU_ICON_RESIZE_WINDOW, mu_rect(rect.x + rect.w - icon_w, rect.y + rect.h - icon_h, icon_w, icon_h), color);

/*
  // short row
  mu_draw_pixel(ctx, bottomx - 2, bottomy - 4, color);
  mu_draw_pixel(ctx, bottomx - 3, bottomy - 3, color);
  mu_draw_pixel(ctx, bottomx - 4, bottomy - 2, color);

  // long row
  mu_draw_pixel(ctx, bottomx - 2, bottomy - 8, color);
  mu_draw_pixel(ctx, bottomx - 3, bottomy - 7, color);
  mu_draw_pixel(ctx, bottomx - 4, bottomy - 6, color);
  mu_draw_pixel(ctx, bottomx - 5, bottomy - 5, color);
  mu_draw_pixel(ctx, bottomx - 6, bottomy - 4, color);
  mu_draw_pixel(ctx, bottomx - 7, bottomy - 3, color);
  mu_draw_pixel(ctx, bottomx - 8, bottomy - 2, color);
*/
}

int mu_begin_window_ex(mu_Context *ctx, const char *title, mu_Rect rect, int opt) {
  mu_Rect body;
  mu_Id id = mu_get_id(ctx, title, strlen(title));
  mu_Container *cnt = get_container(ctx, id, opt);

  if (!cnt || !cnt->open) { return 0; }
  cnt->opt = opt;
  if (cnt->zorder == 0)
    cnt->zorder = (opt & MU_OPT_ALWAYSONTOP) ? 1 : 0;

  push(ctx->id_stack, id);

  if (cnt->rect.w == 0 || opt & MU_OPT_NODRAG) { cnt->rect = rect; }
  begin_root_container(ctx, cnt, opt);
  rect = body = cnt->rect;

  /* draw frame */
  if (~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);
  }

  /* do title bar */
  if (~opt & MU_OPT_NOTITLE) {
    mu_Rect tr = rect;
    tr.h = ctx->style->title_height;
    ctx->draw_frame(ctx, tr, MU_COLOR_TITLEBG);

    /* do title text */
    if (~opt & MU_OPT_NOTITLE) {
      mu_Id title_id = mu_get_id(ctx, "!title", 6);
      mu_update_control(ctx, title_id, tr, opt);
      tr.x += ctx->style->padding + 1;
      // tr.w -= ctx->style->padding;
      mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt, 0, 0);
      if (title_id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT && ~opt & MU_OPT_NODRAG) {
        cnt->rect.x += ctx->mouse_delta.x;
        cnt->rect.y += ctx->mouse_delta.y;
      }

      tr.x -= ctx->style->padding - 1;
      body.y += tr.h; // + ctx->style->padding;
      body.h -= tr.h;
    }

    /* do `close` button */
    if (~opt & MU_OPT_NOCLOSE) {
      mu_Id close_id = mu_get_id(ctx, "!close", 6);
      mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);
      tr.w -= r.w;
      mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);
      mu_update_control(ctx, close_id, r, opt);

      // if (ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus) {
      if (ctx->mouse_up == MU_MOUSE_LEFT && close_id == ctx->focus) {
        cnt->open = 0;
      }
    }
  }

  push_container_body(ctx, cnt, body, opt);

  /* do `resize` handle */
  if (~opt & MU_OPT_NORESIZE) {
    int sz = ctx->style->title_height;
    mu_Id resize_id = mu_get_id(ctx, "!resize", 7);
    mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);
    mu_update_control(ctx, resize_id, r, opt);
    if (resize_id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT) {
      cnt->rect.w = mu_max(96, cnt->rect.w + ctx->mouse_delta.x);
      cnt->rect.h = mu_max(64, cnt->rect.h + ctx->mouse_delta.y);
    }

    // draw_resize_handle(ctx, r);
    mu_draw_icon(ctx, MU_ICON_RESIZE_WINDOW, mu_rect(rect.x + rect.w - 10, rect.y + rect.h - 10, 10, 10), ctx->style->colors[MU_COLOR_BORDER]);
  }

  /* resize to content size */
  if (opt & MU_OPT_AUTOSIZE) {
    mu_Rect r = get_layout(ctx)->body;
    cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);
    cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);
  }

  /* close if this is a popup window and elsewhere was clicked */
  if (opt & MU_OPT_POPUP) {
    if (ctx->mouse_pressed && ctx->hover_root != cnt) {
      // printf("clicked elsewhere\n");
      cnt->open = 0;
      // active_popup_id = 0;
    } else if (ctx->mouse_up && cnt->open++ > 1 && (opt & MU_OPT_HIDE_ON_CLICK)) {
      // printf("hiding popup, mouse up and cnt->open is %d\n", cnt->open);
      cnt->open = 0;
      // active_popup_id = 0;
    // close popup if opened with mouse click and release took longer than X ms
    } else if (ctx->mouse_up == MU_MOUSE_RIGHT && mu_get_timediff(ctx->mouse_down_ts) > 0.2) {
      // printf("closing, last mousedown was at %f\n", mu_get_timediff(ctx->mouse_down_ts));
      cnt->open = 0;
      // active_popup_id = 0;
    }
    if (!cnt->open) ctx->needs_redraw = 1;
  }

  mu_push_clip_rect(ctx, cnt->body);
  return MU_RES_ACTIVE;
}


void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}


void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  cnt->zorder = 2;
  ctx->hover = 0;
  ctx->hover_type = 0;
  ctx->focus = 0;
  mu_bring_to_front(ctx, cnt);
}

void mu_close_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  cnt->open = 0;

  // if (active_popup_id != 0) {
  //   mu_Id id = mu_get_id(ctx, name, strlen(name));
  //   if (active_popup_id == id) {
  //     active_popup_id = 0;
  //   }
  // }
}

int mu_begin_popup(mu_Context *ctx, const char *name, int opt) {
  opt |= (MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED);
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}

int mu_begin_popup_ex(mu_Context *ctx, const char *name, mu_Rect r, int opt) {
  opt |= (MU_OPT_POPUP | MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_CLOSED);
  return mu_begin_window_ex(ctx, name, r, opt);
}

void mu_end_popup(mu_Context *ctx) {
  mu_end_window(ctx);
}

int mu_is_popup_open(mu_Context * ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  return cnt->open && cnt->rect.w > 0; // > 0 ? 1 : 0;

  // mu_Id id = mu_get_id(ctx, name, strlen(name));
  // return active_popup_id != 0 && active_popup_id == id ? 1 : 0;
}

int mu_begin_context_menu(mu_Context * ctx, const char *name, int window_w, int window_h) {

  if (context_menu_x != 0) {
    mu_Container * cnt = mu_get_container(ctx, name);

    // resize rect to detected content size if still not updated (just clicked)
    if (cnt->rect.w == ctx->style->padding * 2 && cnt->rect.h == ctx->style->padding * 2) {
      cnt->rect.w += cnt->content_size.x;
      cnt->rect.h += cnt->content_size.y;
    }

    int distance_to_right = window_w - context_menu_x;
    int distance_to_bottom = window_h - context_menu_y;

    if (distance_to_right < cnt->rect.w) {
      // cnt->rect.x = window_w - cnt->rect.w; // this seems to be the most common behaviour
      cnt->rect.x = context_menu_x - cnt->rect.w; // but this seems nicer
    }

    if (distance_to_bottom < cnt->rect.h) {
      cnt->rect.y = context_menu_y - cnt->rect.h;
    }
  }

  int res = mu_begin_popup(ctx, name, MU_OPT_HIDE_ON_CLICK);

  if (res == 0) {
    context_menu_x = 0;
    context_menu_y = 0;
  } else if (context_menu_x == 0) {
    mu_Container *cnt = mu_get_current_container(ctx);
    // store the original x/y position where the menu was opened, because if we modify it
    // then we'll end up modifying the modified value in the next frame
    context_menu_x = cnt->rect.x;
    context_menu_y = cnt->rect.y;
  }

  return res;
}

void mu_end_context_menu(mu_Context * ctx) {
  mu_end_popup(ctx);
}

void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt) {
  mu_Container *cnt;
  mu_push_id(ctx, name, strlen(name));
  cnt = get_container(ctx, ctx->last_id, opt);
  cnt->rect = mu_layout_next(ctx);
  if (~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  }
  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);
}

void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}

/*

static int32_t gizmo_mode = GS_GUI_TRANSFORM_LOCAL;
static float snap[3] = {0};

struct {const char* str; int32_t option;} modes[] = {
    {"World", GS_GUI_TRANSFORM_WORLD},
    {"Local", GS_GUI_TRANSFORM_LOCAL},
    {NULL}
};

struct {const char* str; float* option; float step;} snaps[] = {
  {"Translation", &snap[0], 0.01f},
  {"Rotation", &snap[1], 1.f},
  {"Scale", &snap[2], 0.01f},
  {NULL}
};

char TMP[256] = {0};

gs_snprintf(TMP, sizeof(TMP), "Mode: %s", modes[gizmo_mode].str);
if (gs_gui_combo_begin(gui, "#mode", TMP, 2)) {
  gs_gui_layout_row(gui, 1, (int[]) {-1}, 0);
  for (uint32_t i = 0; modes[i].str != NULL; ++i) {
    if (gs_gui_button(gui, modes[i].str)) gizmo_mode = i;
  }
  gs_gui_combo_end(gui);
}

if (gs_gui_combo_begin(gui, "#snaps", "Snap", 3)) {
  gs_gui_push_id(gui, "#snap", strlen("#snap"));
  gs_gui_layout_row(gui, 2, (int[]){100, -5}, 0);
  for (uint32_t i = 0; snaps[i].str != NULL; ++i) {
      gs_gui_label(gui, snaps[i].str);
      gs_gui_number(gui, snaps[i].option, snaps[i].step);
  }
  gs_gui_pop_id(gui);
  gs_gui_combo_end(gui);
}


*/

int mu_begin_combo_ex(mu_Context* ctx, const char* id, const char* current_item, int32_t max_items, int32_t opt) {
    opt = MU_OPT_NODRAG | MU_OPT_NORESIZE | MU_OPT_NOTITLE | MU_OPT_HIDE_ON_CLICK;

    if (mu_button(ctx, current_item)) {
        mu_open_popup(ctx, id);
    }

    mu_Rect rect = ctx->last_rect;
    rect.y += rect.h;

    if (max_items > 40) {
      rect.h = max_items;
    } else if (max_items > 0) {
      rect.h = (max_items * (ctx->style->size.y + ctx->style->spacing))
               + (ctx->style->padding * 2)
               - ctx->style->spacing; // remove trailing spacing after last item
    }

    return mu_begin_popup_ex(ctx, id, rect, opt);
}

void mu_end_combo(mu_Context* ctx) {
    mu_end_popup(ctx);
}

int mu_begin_tabs(mu_Context *ctx, const char *name, int num_tabs, const char **tab_names, int *active_tab) {
  mu_Container *cnt;
  int opt = 0;
  mu_push_id(ctx, name, strlen(name));
  cnt = get_container(ctx, ctx->last_id, opt);
  cnt->rect = mu_layout_next(ctx);

  // if (~opt & MU_OPT_NOFRAME) {
  //   ctx->draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  // }

  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);

  int active = -1;
  int active_idx = *active_tab;
  // printf("drawing %d tabs, active one is %d\n", num_tabs, active_idx);

  int i;
  mu_Rect r;
  mu_Id id;
  int idx;
  const char * label;
  int is_active = 0;
  int toggled;
  int toggled_id = -1;

  int tab_width = 70;
  int tab_height = 36;

  mu_layout_row(ctx, 3, (int[]){tab_width, tab_width, tab_width}, 30);

  for (i = 0; i < num_tabs; i++) {
    label = tab_names[i];
    id = mu_get_id(ctx, label, strlen(label));
    idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    is_active = (idx >= 0 || i == active_idx);
    r = mu_layout_next(ctx);

    if (toggled_id > -1) {     // a previous tab has just been toggled
      is_active = toggled = 0; // so mark this one as inactive
    } else {
      mu_update_control(ctx, id, r, 0);
      toggled = (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);
      if (toggled) toggled_id = i;
    }

    if (idx >= 0) {

      if (is_active || toggled) {
        active = i;
        mu_pool_update(ctx, ctx->treenode_pool, idx);
      } else {
        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
      }

    } else if (is_active || toggled) {
      active = i;
      mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    }

    int colorid = MU_COLOR_TABINACTIVE;
    // colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
    colorid += (ctx->hover == id) ? 1 : 0;

    // offset x so first tab starts at x=0
    r.x -= 5;

    // if (!toggled) mu_draw_rect(ctx, r, ctx->style->colors[colorid]);
    // mu_draw_box_outer(ctx, r, ctx->style->colors[MU_COLOR_BORDER], 1, 1);
    ctx->draw_frame(ctx, r, colorid);
    mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0, 0, opt & MU_OPT_RAISED && ctx->focus != id ? -1 : 0);
  }

  // if a tab was toggled but the active one came before
  // then we need to mark that previous active tab as inactive
  if (toggled_id > -1 && active < toggled) {
    label = tab_names[active];
    id = mu_get_id(ctx, label, strlen(label));
    idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    // r = mu_layout_next(ctx);

    memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
    active = toggled_id;
  }

  // r now is the last tabs's rect, meaning the Y coordinates and width are OK
  // we just need to determine the X position
  r.x = cnt->rect.x + ctx->style->padding + (tab_width * active) + (active * ctx->style->spacing) + 2;
  r.x -= 5; // offset so first tab starts at x:0
  r.y -= 3;
  r.h += 4;

  mu_Color tab_bg = ctx->style->colors[MU_COLOR_TABACTIVE];

  // draw tabs container
  ctx->draw_frame(ctx, mu_rect(cnt->rect.x+3, r.y + tab_height - 2, cnt->rect.w - 6, cnt->rect.h + cnt->scroll.y - tab_height - 3), MU_COLOR_TABCONTENT);
  // mu_draw_rect(ctx, mu_rect(cnt->rect.x+3, r.y + tab_height - 2, cnt->rect.w - 6, cnt->rect.h + cnt->scroll.y - tab_height - 2), tab_bg);
  // mu_draw_box_inner(ctx, mu_rect(cnt->rect.x - cnt->scroll.x + 2, r.y + tab_height - 3, cnt->rect.w + cnt->scroll.x - 4, cnt->rect.h + cnt->scroll.y - tab_height), ctx->style->colors[MU_COLOR_BORDER], 1, 1);

  // now draw the active tab
  label = tab_names[active];
  id = mu_get_id(ctx, label, strlen(label));
  // mu_draw_rect(ctx, mu_rect(r.x - cnt->scroll.x, r.y+3, r.w, r.h-3), tab_bg);
  ctx->draw_frame(ctx, mu_rect(r.x - cnt->scroll.x, r.y+3, r.w, r.h-3), MU_COLOR_TABACTIVE);
  mu_draw_control_text(ctx, label, mu_rect(r.x-cnt->scroll.x, r.y, r.w, r.h), MU_COLOR_TEXT, 0, 0, opt & MU_OPT_RAISED && ctx->focus != id ? -1 : 0);

  *active_tab = active;
  return active;
}

void mu_end_tabs(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}

/*
int incrementer(mu_Context *ctx, int *value) {
  mu_Id     id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect rect = mu_layout_next(ctx);
  mu_update_control(ctx, id, rect, 0);

  // handle input
  int res = 0;
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    (*value)++;
    res |= MU_RES_CHANGE;
  }

  // draw
  char buf[32];
  sprintf(buf, "%d", *value);
  mu_draw_control_frame(ctx, id, rect, MU_COLOR_BUTTON, 0);
  mu_draw_control_text(ctx, buf, rect, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER, 0, 0);

  return res;
}
*/
