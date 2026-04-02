// gcc rounded-box.c -lX11 -lXrender -lm -o rounded-box

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Draw a quarter circle at the specified corner
void draw_quarter_circle(Display *dpy, Picture dst, XRenderColor *color,
                         int corner_x, int corner_y, int radius, int corner_type) {
    // corner_type: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right

    Picture solid = XRenderCreateSolidFill(dpy, color);

    // Create an alpha mask picture
    XRenderPictFormat *alpha_fmt = XRenderFindStandardFormat(dpy, PictStandardA8);
    Pixmap mask_pix = XCreatePixmap(dpy, DefaultRootWindow(dpy), radius, radius, 8);
    Picture mask = XRenderCreatePicture(dpy, mask_pix, alpha_fmt, 0, NULL);

    // Clear mask to transparent
    XRenderColor transparent = { .alpha = 0 };
    XRenderFillRectangle(dpy, PictOpSrc, mask, &transparent, 0, 0, radius, radius);

    // Draw the quarter circle
    XRenderColor opaque = { .alpha = 0xFFFF };

    for (int dy = 0; dy < radius; dy++) {
        int dx = sqrt(radius * radius - dy * dy);

        switch(corner_type) {
            case 0: // top-left: quarter circle in bottom-right
                if (dx > 0) {
                    XRenderFillRectangle(dpy, PictOpOver, mask, &opaque,
                                        radius - dx, dy, dx, 1);
                }
                break;

            case 1: // top-right: quarter circle in bottom-left
                if (dx > 0) {
                    XRenderFillRectangle(dpy, PictOpOver, mask, &opaque,
                                        0, dy, dx, 1);
                }
                break;

            case 2: // bottom-left: quarter circle in top-right
                if (dx > 0) {
                    XRenderFillRectangle(dpy, PictOpOver, mask, &opaque,
                                        radius - dx, radius - 1 - dy, dx, 1);
                }
                break;

            case 3: // bottom-right: quarter circle in top-left
                if (dx > 0) {
                    XRenderFillRectangle(dpy, PictOpOver, mask, &opaque,
                                        0, radius - 1 - dy, dx, 1);
                }
                break;
        }
    }

    // Composite the solid color through the mask
    XRenderComposite(dpy, PictOpOver, solid, mask, dst,
                     0, 0, 0, 0,
                     corner_x, corner_y, radius, radius);

    XFreePixmap(dpy, mask_pix);
    XRenderFreePicture(dpy, mask);
    XRenderFreePicture(dpy, solid);
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     100, 100, 400, 300, 0,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));

    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);

    XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
    Picture window_picture = XRenderCreatePicture(dpy, win, fmt, 0, NULL);

    XEvent event;
    while (1) {
        XNextEvent(dpy, &event);
        if (event.type == Expose) {
            int x = 50, y = 50;
            int w = 300, h = 200;
            int radius = 30;

            // Clear to white
            XRenderColor white = { .red = 0xFFFF, .green = 0xFFFF, .blue = 0xFFFF, .alpha = 0xFFFF };
            XRenderFillRectangle(dpy, PictOpSrc, window_picture, &white, 0, 0, 400, 300);

            // Fill color (blue)
            XRenderColor blue = { .red = 0x4000, .green = 0x8000, .blue = 0xFFFF, .alpha = 0xFFFF };

            // Draw the central body (horizontal strip)
            XRenderFillRectangle(dpy, PictOpOver, window_picture, &blue,
                                 x + radius, y,
                                 w - 2 * radius, h);

            // Draw the middle vertical strip
            XRenderFillRectangle(dpy, PictOpOver, window_picture, &blue,
                                 x, y + radius,
                                 w, h - 2 * radius);

            // Draw the four quarter circles
            // Top-left: corner at (x, y)
            draw_quarter_circle(dpy, window_picture, &blue,
                               x, y + h - radius, radius, 0);

            // Top-right: corner at (x + w - radius, y)
            draw_quarter_circle(dpy, window_picture, &blue,
                               x + w - radius, y + h - radius, radius, 1);

            // Bottom-left: corner at (x, y + h - radius)
            draw_quarter_circle(dpy, window_picture, &blue,
                               x, y, radius, 2);

            // Bottom-right: corner at (x + w - radius, y + h - radius)
            draw_quarter_circle(dpy, window_picture, &blue,
                               x + w - radius, y, radius, 3);
        }

        if (event.type == KeyPress)
            break;
    }

    XRenderFreePicture(dpy, window_picture);
    XCloseDisplay(dpy);
    return 0;
}