#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    /* Check XRender extension */
    int event_base, error_base;
    if (!XRenderQueryExtension(dpy, &event_base, &error_base)) {
        fprintf(stderr, "XRender extension not available\n");
        return 1;
    }

    /* Create a simple window */
    Window win = XCreateSimpleWindow(dpy, root, 0, 0, 400, 300, 0,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));
    XMapWindow(dpy, win);
    XSync(dpy, False);

    /* Get visual info for the window's default visual */
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    XRenderPictFormat *format = XRenderFindVisualFormat(dpy, visual);
    if (!format) {
        fprintf(stderr, "No XRender format for visual\n");
        return 1;
    }

    /* Create a picture for the window */
    Picture win_picture = XRenderCreatePicture(dpy, win, format, 0, NULL);
    if (!win_picture) {
        fprintf(stderr, "Failed to create window picture\n");
        return 1;
    }

    /* Fill the window with a red rectangle */
    XRenderColor red = {0xffff, 0x0000, 0x0000, 0xffff};
    XRenderFillRectangle(dpy, PictOpSrc, win_picture, &red,
                         0, 0, 400, 300);

    XFlush(dpy);
    printf("Window should now be red. Press Enter to exit.\n");
    getchar();

    XRenderFreePicture(dpy, win_picture);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}