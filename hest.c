#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xinerama.h>

#define LENGTH(array) (sizeof(array) / sizeof(array[0]))

typedef unsigned char HestWindow;

typedef struct {
    short x;
    short y;
    short w;
    short h;
    HestWindow curwin;
    Window windows[30];
    HestWindow stack[30];
    HestWindow top;
    int cx;
    int cy;
} HestMonitor;

static Display *dpy;
static GC gc;
static unsigned char curmon = 0;
static KeySym monitor_keys[10] = {
    XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8,     XK_9,      XK_0
};
static HestMonitor monitors[12];
static Window pager;
static Window root;
static short screen;
static KeySym window_keys[30] = {
    XK_q, XK_w, XK_e, XK_r, XK_t, XK_y, XK_u, XK_i,     XK_o,      XK_p,
    XK_a, XK_s, XK_d, XK_f, XK_g, XK_h, XK_j, XK_k,     XK_l,      XK_semicolon,
    XK_z, XK_x, XK_c, XK_v, XK_b, XK_n, XK_m, XK_comma, XK_period, XK_slash
};
static const char *current_border_color  = "#bbccff";
static const char *fg_color              = "#ffeedd";
static const char *normal_border_color   = "#445566";
static const char *occupied_bg_color     = "#223344";
static const char *vacant_bg_color       = "#000000";

static void
drawpager(void) {
    char buffer[256];
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor color;
    HestWindow i;
    unsigned short x, y, h, w;
    time_t t;
    struct tm *tmp;
    HestMonitor *mon = &monitors[curmon];

    XMoveResizeWindow(dpy, pager, mon->x, mon->y + mon->h - mon->h/3.5 - 32,
            mon->w, mon->h / 3.5 + 32 + 1);

    XMapRaised(dpy, pager);

    w = (mon->w / 10 - 12);
    h = (mon->h / 10 - 12);

    XSetForeground(dpy, gc, XBlackPixel(dpy, screen));
    XFillRectangle(dpy, pager, gc, 0, 0, mon->w / 3.5,
            mon->h / 3.5 + 32 + 1);

    for(i = 0; i < LENGTH(mon->windows); ++i) {
        x = (i % 10 + 1) * 10 + i % 10 * w;
        y = (i / 10 + 1) * 10 + i / 10 * h;

        strcpy(buffer, XKeysymToString(window_keys[i]));

        if(mon->windows[i])
            XAllocNamedColor(dpy, cmap, occupied_bg_color, &color, &color);
        else
            XAllocNamedColor(dpy, cmap, vacant_bg_color, &color, &color);

        XSetForeground(dpy, gc, color.pixel);
        XFillRectangle(dpy, pager, gc, x, y, w, h);

        if(i == mon->curwin)
            XAllocNamedColor(dpy, cmap, current_border_color, &color, &color);
        else
            XAllocNamedColor(dpy, cmap, normal_border_color, &color, &color);

        XSetForeground(dpy, gc, color.pixel);
        XDrawRectangle(dpy, pager, gc, x, y, w, h);

        XAllocNamedColor(dpy, cmap, fg_color, &color, &color);
        XSetForeground(dpy, gc, color.pixel);
        XDrawString(dpy, pager, gc, x + 8, y + 16, buffer, strlen(buffer));
    }

    t = time(NULL);
    tmp = localtime(&t);
    strftime(buffer, LENGTH(buffer), "%Y-%m-%d %H:%M:%S | %G-%V-%u", tmp);
    XDrawString(dpy, pager, gc, 16, mon->h/3.5 + 26, buffer, strlen(buffer));
}

static void
showhide(void) {
    for(unsigned char m = 0; m < LENGTH(monitors); ++m) {
        HestMonitor *mon = &monitors[m];

        if(mon->windows[mon->curwin]) {
            XMapRaised(dpy, mon->windows[mon->curwin]);
            XMoveResizeWindow(dpy, mon->windows[mon->curwin],
                    mon->x, mon->y, mon->w, mon->h);
        }

        if(m == curmon) {
            if(mon->windows[mon->curwin])
                XSetInputFocus(dpy, mon->windows[mon->curwin],
                        RevertToPointerRoot, CurrentTime);
            else
                XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        }

        for(HestWindow w = 0; w < LENGTH(mon->windows); ++w)
            if(mon->windows[w] && w != mon->curwin)
                XUnmapWindow(dpy, mon->windows[w]);
    }
}

static void
viewmon(unsigned char monitor) {
    curmon = monitor;
    showhide();
}

static void
viewwin(HestWindow window) {
    HestMonitor *mon = &monitors[curmon];

    mon->stack[mon->top = 0] = mon->curwin = window;

    showhide();
}

static void
swapmon(unsigned char a, unsigned char b) {
    HestMonitor *ma = &monitors[a];
    HestMonitor *mb = &monitors[b];

    Window tmp = ma->windows[ma->curwin];
    ma->windows[ma->curwin] = mb->windows[mb->curwin];
    mb->windows[mb->curwin] = tmp;

    showhide();
}

static void
swapwin(HestWindow a, HestWindow b) {
    HestMonitor *mon = &monitors[curmon];

    Window tmp = mon->windows[a];
    mon->windows[a] = mon->windows[b];
    mon->windows[b] = tmp;

    mon->stack[mon->top = 0] = mon->curwin;

    showhide();
}

static int
screen_compare(const void *a, const void *b) {
    const XineramaScreenInfo *sa = (XineramaScreenInfo *)a;
    const XineramaScreenInfo *sb = (XineramaScreenInfo *)b;

    return sa->x_org - sb->x_org;
}

static void
xinerama_setup(void) {
    int numscreens;
    XineramaScreenInfo *screens = XineramaQueryScreens(dpy, &numscreens);

    qsort(screens, numscreens, sizeof(XineramaScreenInfo), screen_compare);

    for(int s = 0; s < (int)LENGTH(monitors) && s < numscreens; ++s) {
        monitors[s].x = screens[s].x_org;
        monitors[s].y = screens[s].y_org;
        monitors[s].w = screens[s].width;
        monitors[s].h = screens[s].height;
    }

    XFree(screens);

    showhide();
}

static void
destroynotify(XEvent *ev) {
    XDestroyWindowEvent *dwe = &ev->xdestroywindow;

    for(unsigned char m = 0; m < LENGTH(monitors); ++m) {
        HestMonitor *mon = &monitors[m];

        for(HestWindow w = 0; w < LENGTH(monitors[curmon].windows); ++w) {
            Window *win = &mon->windows[w];

            if(*win == dwe->window) {
                if(w == mon->curwin && mon->top)
                    mon->curwin = mon->stack[--mon->top];

                memset(win, '\0', sizeof(Window));
                showhide();
                return;
            }
        }
    }
}

static void
keypress(XEvent *ev) {
    unsigned long i;
    KeySym keysym;
    XKeyEvent *kev = &ev->xkey;

    keysym = XKeycodeToKeysym(dpy, (KeyCode)kev->keycode, 0);

    if((kev->state & ControlMask || keysym == XK_Control_L || keysym == XK_Control_R) &&
       (kev->state & Mod4Mask || keysym == XK_Super_L || keysym == XK_Super_R)) {
        drawpager();
    }

    if(!(kev->state & Mod4Mask))
        return;

    switch(keysym) {
    case XK_Delete:
        XDestroyWindow(dpy, monitors[curmon].windows[monitors[curmon].curwin]);
        break;

    case XK_grave:
        xinerama_setup();
        break;

    default:
        for(i = 0; i < LENGTH(window_keys); ++i) {
            if(window_keys[i] == keysym) {
                if(kev->state & ShiftMask)
                    swapwin(monitors[curmon].curwin, i);
                else if(i != monitors[curmon].curwin)
                    viewwin(i);

                break;
            }
        }

        for(i = 0; i < LENGTH(monitor_keys); ++i) {
            if(monitor_keys[i] == keysym && monitors[i].w) {
                if(kev->state & ShiftMask)
                    swapmon(curmon, i);
                else if(i != curmon)
                    viewmon(i);

                break;
            }
        }
    }
}

static void
keyrelease(XEvent *ev) {
    KeySym keysym;
    XKeyEvent *kev = &ev->xkey;

    keysym = XKeycodeToKeysym(dpy, (KeyCode)kev->keycode, 0);

    if(keysym == XK_Control_L || keysym == XK_Control_R ||
       keysym == XK_Super_L || keysym == XK_Super_R)
        XUnmapWindow(dpy, pager);
}

static void
maprequest(XEvent *ev) {
    static XWindowAttributes wa;
    XMapRequestEvent *mrev = &ev->xmaprequest;
    HestMonitor *mon = &monitors[curmon];

    if(!XGetWindowAttributes(dpy, mrev->window, &wa))
        return;

    for(HestWindow w = 0; w < LENGTH(mon->windows); ++w)
        if(mon->windows[w] == mrev->window) {
            showhide();
            return;
        }

    if(mon->windows[mon->curwin])
        for(mon->curwin = 0;
            mon->curwin < LENGTH(mon->windows) && mon->windows[mon->curwin];
            ++mon->curwin);

    mon->stack[++mon->top] = mon->curwin;
    mon->windows[mon->curwin] = mrev->window;
    showhide();
}

static int
xerror(Display *dpy, XErrorEvent *ee) {
    fprintf(stderr, "Got an XErrorEvent: %p, %p\n", (void *)dpy, (void *)ee);

    return 0;
}

static void
setup(void) {
    unsigned char i;
    static const KeySym modifiers[] = {
        Mod4Mask,
        Mod4Mask | ShiftMask,
        Mod4Mask | ShiftMask | ControlMask,
        Mod4Mask |             ControlMask,
        Mod4Mask                           | Mod5Mask,
        Mod4Mask | ShiftMask               | Mod5Mask,
        Mod4Mask | ShiftMask | ControlMask | Mod5Mask,
        Mod4Mask |             ControlMask | Mod5Mask,
    };

    if(!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    XSetErrorHandler(xerror);

    memset(&monitors, 0, sizeof(monitors));
    for(i = 0; i < LENGTH(monitors); ++i)
        monitors[i].cx = monitors[i].cy = -1;

    XWindowAttributes wa;
    XGetWindowAttributes(dpy, root, &wa);
    monitors[0].w = wa.width;
    monitors[0].h = wa.height;

    xinerama_setup();
    HestMonitor *mon = &monitors[curmon];
    pager = XCreateSimpleWindow(dpy, root, 0, mon->h,
                                mon->w, mon->w/3.5 + 32 + 1, 0,
                                WhitePixel(dpy, screen),
                                BlackPixel(dpy, screen));

    gc = XCreateGC(dpy, pager, 0, NULL);

    XSelectInput(dpy, root, KeyPressMask
            | SubstructureNotifyMask
            | SubstructureRedirectMask);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Super_L), AnyModifier, root,
            False, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Super_R), AnyModifier, root,
            False, GrabModeAsync, GrabModeAsync);
    for(i = 0; i < LENGTH(modifiers); ++i)
        XGrabKey(dpy, AnyKey, modifiers[i], root, True, GrabModeAsync,
                GrabModeAsync);
}

int
main(void) {
    static void(*handler[LASTEvent])(XEvent *) = {
        [DestroyNotify] = destroynotify,
        [KeyPress] = keypress,
        [KeyRelease] = keyrelease,
        [MapRequest] = maprequest
    };
    XEvent ev;

    setup();

    while(!XNextEvent(dpy, &ev))
        if(handler[ev.type])
            handler[ev.type](&ev);

    return EXIT_SUCCESS;
}
