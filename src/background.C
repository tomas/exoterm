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

typedef struct ShadingInfo {
  XColor tintColor;
  int shading;
} ShadingInfo;

bool
image_effects::set_blur (const char *geom)
{
  bool changed = false;
  unsigned int hr, vr;
  int junk;
  int geom_flags = XParseGeometry (geom, &junk, &junk, &hr, &vr);

  if (!(geom_flags & WidthValue))
    hr = 1;
  if (!(geom_flags & HeightValue))
    vr = hr;

  min_it (hr, 128);
  min_it (vr, 128);

  if (h_blurRadius != hr)
    {
      changed = true;
      h_blurRadius = hr;
    }

  if (v_blurRadius != vr)
    {
      changed = true;
      v_blurRadius = vr;
    }

  return changed;
}

bool
image_effects::set_tint (const rxvt_color &new_tint)
{
  if (!tint_set || tint != new_tint)
    {
      tint = new_tint;
      tint_set = true;

      return true;
    }

  return false;
}

bool
image_effects::set_shade (const char *shade_str)
{
  int new_shade = atoi (shade_str);

  clamp_it (new_shade, -100, 200);
  if (new_shade < 0)
    new_shade = 200 - (100 + new_shade);

  if (new_shade != shade)
    {
      shade = new_shade;
      return true;
    }

  return false;
}

void ShadeXImage(Display * dpy, XImage* srcImage, ShadingInfo* shading, GC gc ) {
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

  /* boring lookup table pre-initialization */
  switch (srcImage->bits_per_pixel) {
    case 15:
      if ((mask_r != 0x7c00) ||
          (mask_g != 0x03e0) ||
          (mask_b != 0x001f))
        return;
        lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(32+32+32));
        lookup_r = lookup;
        lookup_g = lookup+32;
        lookup_b = lookup+32+32;
        sh_r = 10;
        sh_g = 5;
        sh_b = 0;
      break;
    case 16:
      if ((mask_r != 0xf800) ||
          (mask_g != 0x07e0) ||
          (mask_b != 0x001f))
        return;
        lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(32+64+32));
        lookup_r = lookup;
        lookup_g = lookup+32;
        lookup_b = lookup+32+64;
        sh_r = 11;
        sh_g = 5;
        sh_b = 0;
      break;
    case 24:
      if ((mask_r != 0xff0000) ||
          (mask_g != 0x00ff00) ||
          (mask_b != 0x0000ff))
        return;
        lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(256+256+256));
        lookup_r = lookup;
        lookup_g = lookup+256;
        lookup_b = lookup+256+256;
        sh_r = 16;
        sh_g = 8;
        sh_b = 0;
      break;
    case 32:
      if ((mask_r != 0xff0000) ||
          (mask_g != 0x00ff00) ||
          (mask_b != 0x0000ff))
        return;
        lookup = (u_int32_t *) malloc (sizeof (u_int32_t)*(256+256+256));
        lookup_r = lookup;
        lookup_g = lookup+256;
        lookup_b = lookup+256+256;
        sh_r = 16;
        sh_g = 8;
        sh_b = 0;
      break;
    default:
      return; /* we do not support this color depth */
  }

  /* prepare limits for color transformation (each channel is handled separately) */
  if (shading->shading < 0) {
    int shade;
    shade = -shading->shading;
    if (shade < 0) shade = 0;
    if (shade > 100) shade = 100;

    lower_lim_r = 65535-shading->tintColor.red;
    lower_lim_g = 65535-shading->tintColor.green;
    lower_lim_b = 65535-shading->tintColor.blue;

    lower_lim_r = 65535-(unsigned int)(((u_int32_t)lower_lim_r)*((u_int32_t)shade)/100);
    lower_lim_g = 65535-(unsigned int)(((u_int32_t)lower_lim_g)*((u_int32_t)shade)/100);
    lower_lim_b = 65535-(unsigned int)(((u_int32_t)lower_lim_b)*((u_int32_t)shade)/100);

    upper_lim_r = upper_lim_g = upper_lim_b = 65535;
  } else {
    int shade;
    shade = shading->shading;
    if (shade < 0) shade = 0;
    if (shade > 100) shade = 100;

    lower_lim_r = lower_lim_g = lower_lim_b = 0;

    upper_lim_r = (unsigned int)((((u_int32_t)shading->tintColor.red)*((u_int32_t)shading->shading))/100);
    upper_lim_g = (unsigned int)((((u_int32_t)shading->tintColor.green)*((u_int32_t)shading->shading))/100);
    upper_lim_b = (unsigned int)((((u_int32_t)shading->tintColor.blue)*((u_int32_t)shading->shading))/100);
  }

  /* switch red and blue bytes if necessary, we need it for some weird XServers like XFree86 3.3.3.1 */
  if ((srcImage->bits_per_pixel == 24) && (mask_r >= 0xFF0000 ))
  {
    unsigned int tmp;

    tmp = lower_lim_r;
    lower_lim_r = lower_lim_b;
    lower_lim_b = tmp;

    tmp = upper_lim_r;
    upper_lim_r = upper_lim_b;
    upper_lim_b = tmp;
  }

  /* fill our lookup tables */
  for (i = 0; i <= mask_r>>sh_r; i++)
  {
    u_int32_t tmp;
    tmp = ((u_int32_t)i)*((u_int32_t)(upper_lim_r-lower_lim_r));
    tmp += ((u_int32_t)(mask_r>>sh_r))*((u_int32_t)lower_lim_r);
    lookup_r[i] = (tmp/65535)<<sh_r;
  }
  for (i = 0; i <= mask_g>>sh_g; i++)
  {
    u_int32_t tmp;
    tmp = ((u_int32_t)i)*((u_int32_t)(upper_lim_g-lower_lim_g));
    tmp += ((u_int32_t)(mask_g>>sh_g))*((u_int32_t)lower_lim_g);
    lookup_g[i] = (tmp/65535)<<sh_g;
  }
  for (i = 0; i <= mask_b>>sh_b; i++)
  {
    u_int32_t tmp;
    tmp = ((u_int32_t)i)*((u_int32_t)(upper_lim_b-lower_lim_b));
    tmp += ((u_int32_t)(mask_b>>sh_b))*((u_int32_t)lower_lim_b);
    lookup_b[i] = (tmp/65535)<<sh_b;
  }

  /* apply table to input image (replacing colors by newly calculated ones) */
  switch (srcImage->bits_per_pixel)
  {
    case 15:
    {
      unsigned short *p1, *pf, *p, *pl;
      p1 = (unsigned short *) srcImage->data;
      pf = (unsigned short *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf)
      {
        p = p1;
        pl = p1 + srcImage->width;
        for (; p < pl; p++)
        {
          *p = lookup_r[(*p & 0x7c00)>>10] |
               lookup_g[(*p & 0x03e0)>> 5] |
               lookup_b[(*p & 0x001f)];
        }
        p1 = (unsigned short *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
    case 16:
    {
      unsigned short *p1, *pf, *p, *pl;
      p1 = (unsigned short *) srcImage->data;
      pf = (unsigned short *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf)
      {
        p = p1;
        pl = p1 + srcImage->width;
        for (; p < pl; p++)
        {
          *p = lookup_r[(*p & 0xf800)>>11] |
               lookup_g[(*p & 0x07e0)>> 5] |
               lookup_b[(*p & 0x001f)];
        }
        p1 = (unsigned short *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
    case 24:
    {
      unsigned char *p1, *pf, *p, *pl;
      p1 = (unsigned char *) srcImage->data;
      pf = (unsigned char *) (srcImage->data + srcImage->height * srcImage->bytes_per_line);
      while (p1 < pf)
      {
        p = p1;
        pl = p1 + srcImage->width * 3;
        for (; p < pl; p += 3)
        {
          p[0] = lookup_r[(p[0] & 0xff0000)>>16];
          p[1] = lookup_r[(p[1] & 0x00ff00)>> 8];
          p[2] = lookup_r[(p[2] & 0x0000ff)];
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

      while (p1 < pf)
      {
        p = p1;
        pl = p1 + srcImage->width;
        for (; p < pl; p++)
        {
          *p = lookup_r[(*p & 0xff0000)>>16] |
               lookup_g[(*p & 0x00ff00)>> 8] |
               lookup_b[(*p & 0x0000ff)] |
               (*p & ~0xffffff);
        }
        p1 = (u_int32_t *) ((char *) p1 + srcImage->bytes_per_line);
      }
      break;
    }
  }

  free (lookup);
}

void CopyAndShadeArea(Display * dpy, Drawable src, Pixmap target, int x, int y, int w, int h, int trg_x, int trg_y, GC gc, ShadingInfo* shading) {
  // int (*oldXErrorHandler)(Display *, XErrorEvent *);
  if (x < 0 || y < 0) return;

  if (shading) {
    XImage * img = XGetImage(dpy, src, x, y, w, h, AllPlanes, ZPixmap);

    if (img != NULL) {
      ShadeXImage(dpy, img, shading, gc);
      XPutImage(dpy, target, gc, img, 0, 0, trg_x, trg_y, w, h);
      XDestroyImage(img);
      return ;
    }
  }

  if (!XCopyArea(dpy, src, target, gc, x, y, w, h, trg_x, trg_y))
    XFillRectangle(dpy, target, gc, trg_x, trg_y, w, h);
}

Pixmap ShadePixmap(Display * dpy, Window win, Pixmap src, int x, int y, int width, int height, GC gc, ShadingInfo* shading, int depth) {
  Pixmap target = XCreatePixmap(dpy, win, width, height, depth);

  if (target != None) {
    CopyAndShadeArea(dpy, src, target, x, y, width, height, 0, 0, gc, shading);
  }

  return target;
}


Pixmap getRootPixmap(Display *dpy) {
  Pixmap root_pixmap = None;
  static Atom id = None;
  const char* pixmap_id_names[] = {
    "_XROOTPMAP_ID", "ESETROOT_PMAP_ID", NULL
  };

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

Pixmap load_root_img(Display * dpy, Window win, GC gc, int * w_out, int * h_out) {
  Pixmap bg = getRootPixmap(dpy);
  if (bg == None) {
    printf("unable to get root pixmap\n");
    // return 1;
    return NULL;
  }

  Window rootwin;
  int xpos, ypos;
  unsigned int w, h, border_width, depth;

  // get bg image size
  XGetGeometry(dpy, bg, &rootwin, &xpos, &ypos, &w, &h, &border_width, &depth);

  // shade it
  ShadingInfo shade;
  shade.shading = 20;
  shade.tintColor.red   = 0x0000; 
  shade.tintColor.green = 0xFFFF; 
  shade.tintColor.blue  = 0xFFFF;
  Pixmap pix = ShadePixmap(dpy, win, bg, 0, 0, w, h, gc, &shade, depth);

  printf("freeing pixmap\n");
  // XFreePixmap(dpy, bg);

  *w_out = w;
  *h_out = h;

  return pix;
}

void
rxvt_term::bg_render ()
{
  if (bg_flags & BG_INHIBIT_RENDER)
    return;

  // delete bg_img;
  // bg_img = 0;
  // bg_flags = 0;

  if (!mapped)
    return;

# if BG_IMAGE_FROM_ROOT
  if (root_img)
    {

      int x, y;
      getCoords(dpy, parent, &x, &y);
      // printf("got coords: %d/%d -- %d/%d\n", x, y, width, height);
      XCopyArea(dpy, root_img, winbg, gc, x, y, width, height, 0, 0);

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
#if BG_IMAGE_FROM_ROOT
  if (option (Opt_transparent))
    {

      int w, h;
      root_img = load_root_img(dpy, parent, gc, &w, &h);

      printf("creating pixmap: %d/%d, depth: %d\n", w, h, depth);
      winbg = XCreatePixmap(dpy, parent, w, h, depth);

      // if (rs [Rs_blurradius])
      //   root_effects.set_blur (rs [Rs_blurradius]);

      // if (ISSET_PIXCOLOR (Color_tint))
      //   root_effects.set_tint (lookup_color(Color_tint, pix_colors_focused));

      // if (rs [Rs_shade])
      //   root_effects.set_shade (rs [Rs_shade]);

      // rxvt_img::new_from_root (this)->replace (root_img);
      XSelectInput (dpy, display->root, PropertyChangeMask);
      rootwin_ev.start (display, display->root);
    }
#endif
}

#endif /* HAVE_BG_PIXMAP */
