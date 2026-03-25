/*----------------------------------------------------------------------*
 * File:  background.C
 *----------------------------------------------------------------------*
 *
 * All portions of code are copyright by their respective author/s.
 * Copyright (c) 2005-2008 Marc Lehmann <schmorp@schmorp.de>
 * Copyright (c) 2007      Sasha Vasko <sasha@aftercode.net>
 * Copyright (c) 2010-2012 Emanuele Giaquinta <e.giaquinta@glauco.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *---------------------------------------------------------------------*/

#include "../config.h"    /* NECESSARY */
#include "rxvt.h"   /* NECESSARY */

#ifdef HAVE_BG_PIXMAP

/* Alpha-blend the root image with a solid background colour.
 * opacity=0   → pure root image (fully transparent)
 * opacity=100 → solid bg colour (fully opaque)
 * out_channel = root_channel * (1 - opacity/100) + bg_channel * (opacity/100)
 */
void ShadeXImage(Display * dpy, XImage* srcImage, XColor *bg, int opacity, int darken, GC gc) {
  int sh_r, sh_g, sh_b;
  u_int32_t mask_r, mask_g, mask_b;
  u_int32_t *lookup, *lookup_r, *lookup_g, *lookup_b;
  unsigned int lower_lim_r, lower_lim_g, lower_lim_b;
  unsigned int upper_lim_r, upper_lim_g, upper_lim_b;
  int i;

  Visual* visual = DefaultVisual(dpy, DefaultScreen(dpy));

  // if (visual->class != TrueColor || srcImage->format != ZPixmap) return;

  /* for convenience */
  mask_r = visual->red_mask;
  mask_g = visual->green_mask;
  mask_b = visual->blue_mask;

  switch (srcImage->bits_per_pixel) {
    case 15:
      if ((mask_r != 0x7c00) || (mask_g != 0x03e0) || (mask_b != 0x001f)) return;
      lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(32+32+32));
      lookup_r = lookup; lookup_g = lookup+32; lookup_b = lookup+32+32;
      sh_r = 10; sh_g = 5; sh_b = 0;
      break;
    case 16:
      if ((mask_r != 0xf800) || (mask_g != 0x07e0) || (mask_b != 0x001f)) return;
      lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(32+64+32));
      lookup_r = lookup; lookup_g = lookup+32; lookup_b = lookup+32+64;
      sh_r = 11; sh_g = 5; sh_b = 0;
      break;
    case 24:
    case 32:
      if ((mask_r != 0xff0000) || (mask_g != 0x00ff00) || (mask_b != 0x0000ff)) return;
      lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(256+256+256));
      lookup_r = lookup; lookup_g = lookup+256; lookup_b = lookup+256+256;
      sh_r = 16; sh_g = 8; sh_b = 0;
      break;
    default:
      return;
  }

  /* Single-pass blend:
   *   out = wallpaper*(1-darken/100)*(1-opacity/100) + bg*(opacity/100)
   * lower_lim (wallpaper=0): bg_channel * opacity/100
   * upper_lim (wallpaper=max): 65535*(1-darken/100)*(1-opacity/100) + lower_lim */
  lower_lim_r = (u_int32_t)bg->red   * opacity / 100;
  lower_lim_g = (u_int32_t)bg->green * opacity / 100;
  lower_lim_b = (u_int32_t)bg->blue  * opacity / 100;
  upper_lim_r = (u_int32_t)65535 * (100 - darken) * (100 - opacity) / 10000 + lower_lim_r;
  upper_lim_g = (u_int32_t)65535 * (100 - darken) * (100 - opacity) / 10000 + lower_lim_g;
  upper_lim_b = (u_int32_t)65535 * (100 - darken) * (100 - opacity) / 10000 + lower_lim_b;

  /* fill lookup tables */
  for (i = 0; i <= (int)(mask_r >> sh_r); i++) {
    u_int32_t tmp = (u_int32_t)i * (upper_lim_r - lower_lim_r);
    tmp += (u_int32_t)(mask_r >> sh_r) * lower_lim_r;
    lookup_r[i] = (tmp / 65535) << sh_r;
  }
  for (i = 0; i <= (int)(mask_g >> sh_g); i++) {
    u_int32_t tmp = (u_int32_t)i * (upper_lim_g - lower_lim_g);
    tmp += (u_int32_t)(mask_g >> sh_g) * lower_lim_g;
    lookup_g[i] = (tmp / 65535) << sh_g;
  }
  for (i = 0; i <= (int)(mask_b >> sh_b); i++) {
    u_int32_t tmp = (u_int32_t)i * (upper_lim_b - lower_lim_b);
    tmp += (u_int32_t)(mask_b >> sh_b) * lower_lim_b;
    lookup_b[i] = (tmp / 65535) << sh_b;
  }

  switch (srcImage->bits_per_pixel)
  {
    case 15:
    case 16:
    {
      unsigned short *p1, *pf, *p, *pl;
      int rsh = (srcImage->bits_per_pixel == 15) ? 10 : 11;
      p1 = (unsigned short *) srcImage->data;
      pf = (unsigned short *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf) {
        p = p1; pl = p1 + srcImage->width;
        for (; p < pl; p++)
          *p = lookup_r[(*p & mask_r) >> rsh] |
               lookup_g[(*p & mask_g) >>  5] |
               lookup_b[(*p & mask_b)];
        p1 = (unsigned short *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
    case 24:
    {
      /* 24bpp: three separate bytes per pixel, order depends on server byte order.
       * lookup tables are indexed by channel value (0-255) and store pre-shifted results. */
      unsigned char *p1, *pf, *p, *pl;
      p1 = (unsigned char *) srcImage->data;
      pf = (unsigned char *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf) {
        p = p1; pl = p1 + srcImage->width * 3;
        for (; p < pl; p += 3) {
          p[0] = lookup_r[p[0]] >> sh_r;
          p[1] = lookup_g[p[1]] >> sh_g;
          p[2] = lookup_b[p[2]];
        }
        p1 = (unsigned char *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
    case 32:
    {
      u_int32_t *p1, *pf, *p, *pl;
      p1 = (u_int32_t *) srcImage->data;
      pf = (u_int32_t *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf) {
        p = p1; pl = p1 + srcImage->width;
        for (; p < pl; p++)
          *p = lookup_r[(*p & 0xff0000) >> 16] |
               lookup_g[(*p & 0x00ff00) >>  8] |
               lookup_b[(*p & 0x0000ff)] |
               (*p & ~0xffffff);
        p1 = (u_int32_t *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
  }

  free (lookup);
}

void CopyAndShadeArea(Display * dpy, Drawable src, Pixmap target, int x, int y, int w, int h, int trg_x, int trg_y, GC gc, XColor *bg, int opacity, int darken) {
  if (x < 0 || y < 0) return;

  XImage * img = XGetImage(dpy, src, x, y, w, h, AllPlanes, ZPixmap);
  if (img != NULL) {
    ShadeXImage(dpy, img, bg, opacity, darken, gc);
    XPutImage(dpy, target, gc, img, 0, 0, trg_x, trg_y, w, h);
    XDestroyImage(img);
  }
}

Pixmap ShadePixmap(Display * dpy, Window win, Pixmap src, int x, int y, int width, int height, GC gc, XColor *bg, int opacity, int darken, int depth) {
  Pixmap target = XCreatePixmap(dpy, win, width, height, depth);

  if (target != None)
    CopyAndShadeArea(dpy, src, target, x, y, width, height, 0, 0, gc, bg, opacity, darken);

  return target;
}

const char* pixmap_id_names[] = {
  "ESETROOT_PMAP_ID", "_XROOTPMAP_ID", NULL
};

Pixmap getRootPixmap(Display *dpy) {
  Pixmap root_pixmap = None;
  static Atom id = None;

  int i = 0;
  for (i = 0; (pixmap_id_names[i] && (None == root_pixmap)); i++) {

    if (None == id) {
      id = XInternAtom(dpy, pixmap_id_names[i], True);
    }

    if (None != id) {
      Atom actual_type = None;
      int actual_format = 0;
      unsigned long nitems = 0;
      unsigned long bytes_after = 0;
      unsigned char *properties = NULL;
      int rc = 0;

      rc = XGetWindowProperty(dpy, DefaultRootWindow(dpy), id, 0, 1,
                              False, XA_PIXMAP, &actual_type, &actual_format, &nitems,
                              &bytes_after, &properties);

      if (Success == rc && properties) {
        root_pixmap = *((Pixmap*)properties);
      } else {
        printf("Couldn't get %s\n", pixmap_id_names[i]);
      }
    }
  }

  return root_pixmap;
}

void getCoords(Display * dpy, Window win, int * x, int * y) {
  Window child;
  XWindowAttributes xwa;

  // these are for querying
  Window rootwin, parentwin, *children;
  int xpos, ypos;
  unsigned int width, height, border_width, depth, nchildren;

  XQueryTree(dpy, win, &rootwin, &parentwin, &children, &nchildren);
  XQueryTree(dpy, parentwin, &rootwin, &parentwin, &children, &nchildren);
  XGetGeometry(dpy, parentwin, &rootwin, &xpos, &ypos, &width, &height, &border_width, &depth);

  XTranslateCoordinates(dpy, win, rootwin, 0, 0, &xpos, &ypos, &child);
  XGetWindowAttributes(dpy, win, &xwa);
  xpos -= xwa.x;
  ypos -= xwa.y;

  *x = xpos;
  *y = ypos;
}

///////////////////

void
rxvt_term::bg_destroy ()
{
# if BG_IMAGE_FROM_ROOT
  if (root_img != None) XFreePixmap(dpy, root_img);
  root_img = NULL;
# endif
}

bool
rxvt_term::bg_window_size_sensitive ()
{
# if BG_IMAGE_FROM_ROOT
  if (root_img)
    return true;
# endif

  return false;
}

bool
rxvt_term::bg_window_position_sensitive ()
{
# if BG_IMAGE_FROM_ROOT
  if (root_img)
    return true;
# endif

  return false;
}

Pixmap load_root_img(Display * dpy, Window win, GC gc, int * w_out, int * h_out, XColor *bg, int opacity, int darken) {
  Pixmap root_pix = getRootPixmap(dpy);
  if (root_pix == None) {
    printf("Unable to get root pixmap\n");
    return None;
  }

  int xpos, ypos;
  unsigned int w, h, border_width, depth;
  XGetGeometry(dpy, root_pix, &win, &xpos, &ypos, &w, &h, &border_width, &depth);

  Pixmap pix = ShadePixmap(dpy, win, root_pix, 0, 0, w, h, gc, bg, opacity, darken, depth);

  *w_out = w;
  *h_out = h;

  return pix;
}

void
rxvt_term::bg_render ()
{

  if (bg_flags & BG_INHIBIT_RENDER) {
    return;
  }

  // delete bg_img;
  // bg_img = 0;
  // bg_flags = 0;

  if (!mapped) {
    return;
  }

# if BG_IMAGE_FROM_ROOT
  if (root_img)
    {
      int x, y;
      getCoords(dpy, vt, &x, &y);
      // printf("Copying area: %d/%d - %d/%d\n", x, y, width, height);
      int x_padding = window_vt_x * 2;
      int y_padding = window_vt_y * 2;
      XCopyArea(dpy, root_img, winbg, gc, x, y, width + x_padding, height + y_padding, 0, 0);
      bg_flags |= BG_IS_TRANSPARENT;
    }
# endif

  scr_recolor (false);
  bg_flags |= BG_NEEDS_REFRESH;
  bg_valid_since = ev::now ();
}

void
rxvt_term::bg_init ()
{
  /* Parse bgOpacity/bgDarken unconditionally — used by both fake transparency
     (winbg path) and real compositor-based transparency (depth-32 path). */
  if (rs [Rs_bgOpacity])
    {
      bg_opacity = atoi (rs [Rs_bgOpacity]);
      clamp_it (bg_opacity, 0, 100);
    }
  else
    {
#if XFT
      rgba bg_rgba;
      lookup_color (Color_bg, pix_colors_focused).get (bg_rgba);
      if (bg_rgba.a < rgba::MAX_CC)
        bg_opacity = (int)((u_int32_t)bg_rgba.a * 100 / rgba::MAX_CC);
#endif
    }

  if (rs [Rs_bgDarken])
    {
      bg_darken = atoi (rs [Rs_bgDarken]);
      clamp_it (bg_darken, 0, 100);
    }

#if BG_IMAGE_FROM_ROOT
  if (option (Opt_transparent)) {
      /* Free previous resources before recreating */
      if (root_img != None) { XFreePixmap (dpy, root_img); root_img = None; }
      if (winbg    != None) { XFreePixmap (dpy, winbg);    winbg    = None; }

      XColor bg;
      lookup_color (Color_bg, pix_colors_focused).get (bg);

      int w, h;
      root_img = load_root_img (dpy, parent, gc, &w, &h, &bg, bg_opacity, bg_darken);
      if (root_img) winbg = XCreatePixmap (dpy, parent, w, h, depth);

      XSelectInput (dpy, display->root, PropertyChangeMask);
      rootwin_ev.start (display, display->root);
    }
#endif
}

#endif /* HAVE_BG_PIXMAP */
