/* SecureZone - A movie inspired screen locker written in C, using Xlib and PAM
 *
 * Copyright 2015 Pontus Andersson
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/dpms.h>
#include <security/pam_appl.h>

#define MAX_INPUTLEN 256
#define MAX_WILDCARDS 32

typedef struct {
    int n;
    Window root, win;
    GC gc;
    int x_org, y_org, width, height;
    int ix, iy, iw, ih; /* inputfield */
} XScreen;

CARD16 dpms_info, dpms_standby, dpms_suspend, dpms_off;
BOOL use_dpms;

Display *dpy;
XScreen *screens;
int num_screens;

XImage *message, *granted, *denied;
unsigned long bgcolor, fgcolor;
char input[MAX_INPUTLEN];
int inputlen, activated;

void cleanup(void);
void exit_error(const char *error_str, ...);
void event_loop(void);
int handle_event(void);
void toggle_dpms(void);
void init_graphics(void);
void clear_graphics(void);
void draw_message(int direct);
void draw_inputfield(int direct);
void draw_access_blank(int direct);
void draw_access(XImage *image, int direct);
void draw_input(int direct);
void update_screens(void);
int check_input(void);
int pam_check_access(void);
int pam_input_conv(int n, const struct pam_message **msg, struct pam_response **resp, void *d);

/*int count = 0;*/
XImage *__load_ximage(int w, int h, char *d)
{
    char *p, *image;
    int c = w * h;
    image = p = malloc(c * 4 + 1);
    while(c-- > 0) {
        p[0] = (((d[2] - 33) & 0x3) << 6) | ((d[3] - 33));
        p[1] = (((d[1] - 33) & 0xF) << 4) | ((d[2] - 33) >> 2);
        p[2] = ((d[0] - 33) << 2) | ((d[1] - 33) >> 4);
        p[3] = 0;
        d += 4; p += 4;
    }

    return XCreateImage (dpy, XDefaultVisual(dpy, DefaultScreen(dpy)),
            XDefaultDepth(dpy, DefaultScreen(dpy)), ZPixmap, 0, image, w, h, 32, 0);
}

#define XIMAGE_HEADER_BEGIN(name) XImage * load_ximage_##name () {
#define XIMAGE_HEADER_END return __load_ximage(width, height, header_data); }

XIMAGE_HEADER_BEGIN(message)
#include "images/message.h"
    XIMAGE_HEADER_END

XIMAGE_HEADER_BEGIN(granted)
#include "images/access_granted.h"
    XIMAGE_HEADER_END

XIMAGE_HEADER_BEGIN(denied)
#include "images/access_denied.h"
    XIMAGE_HEADER_END


int main(int argc, char **argv)
{
    XEvent ev;
    XSetWindowAttributes wa = {0};
    XineramaScreenInfo *xsi;
    int n, xsi_num;

    XColor black, white;
    char empty_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
    Pixmap empty_pm;
    Cursor cursor;

    if(argc == 2 && strcmp(argv[1], "-v") == 0) {
        printf("securezone-%s, Copyright 2015 Pontus Andersson\n", VERSION);
        exit(EXIT_SUCCESS);
    }

    /* Check if to start blank */
    if(argc == 2 && strcmp(argv[1], "-b") == 0)
        activated = 0;
    else
        activated = 1;

    inputlen = 0;

    if((dpy = XOpenDisplay(0)) == NULL)
        exit_error("Could not open display");

    if(DPMSCapable(dpy)) {
        DPMSInfo(dpy, &dpms_info, &use_dpms);
        if(use_dpms)
            DPMSGetTimeouts(dpy, &dpms_standby, &dpms_suspend, &dpms_off);
    } else {
        use_dpms = False;
    }

    black.red = 0x0;    black.green = 0;      black.blue = 0;
    white.red = 0xFFFF; white.green = 0xFFFF; white.blue = 0xFFFF;
    XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &black);
    XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &white);
    bgcolor = black.pixel;
    fgcolor = white.pixel;

    num_screens = ScreenCount(dpy);
    screens = malloc(sizeof(Screen) * num_screens);
    for(n = 0; n < num_screens; n++) {
        screens[n].n = 0;
        screens[n].root = RootWindow(dpy, n);
        screens[n].width = 0;

        if(XineramaIsActive(dpy)) {
            xsi = (XineramaScreenInfo *) XineramaQueryScreens(dpy, &xsi_num);
            if(xsi_num > 0) {
                screens[n].x_org = xsi[0].x_org;
                screens[n].y_org = xsi[0].y_org;
                screens[n].width = xsi[0].width;
                screens[n].height = xsi[0].height;
            }
            free(xsi);
        }

        if(!screens[n].width) {
            screens[n].x_org = 0;
            screens[n].y_org = 0;
            screens[n].width = DisplayWidth(dpy, n);
            screens[n].height = DisplayHeight(dpy, n);
        }

        screens[n].gc = XCreateGC(dpy, screens[n].root, 0, NULL);
        wa.override_redirect = 1;
        wa.background_pixel = bgcolor;
        screens[n].win = XCreateWindow(dpy, screens[n].root, 0, 0,
                DisplayWidth(dpy, n), DisplayHeight(dpy, n), 0,
                DefaultDepth(dpy, n), CopyFromParent,
                DefaultVisual(dpy, n), CWOverrideRedirect | CWBackPixel, &wa);

        /* Hide cursor */
        empty_pm = XCreateBitmapFromData(dpy, screens[n].win, empty_data, 8, 8);
        cursor = XCreatePixmapCursor(dpy, empty_pm, empty_pm, &black, &black, 0, 0);
        XSelectInput(dpy, screens[n].win, ExposureMask);
        XSelectInput(dpy, screens[n].root, SubstructureNotifyMask);
        XDefineCursor(dpy, screens[n].win, cursor);

        /* Map window and wait for the event */
        XMapWindow(dpy, screens[n].win);
        for(; ev.type != MapNotify; XNextEvent(dpy, &ev));

        /* Grab keyboard and mouse */
        while(XGrabKeyboard(dpy, screens[n].root, True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess)
            usleep(1000);
        while(XGrabPointer(dpy, screens[n].root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime) != GrabSuccess)
            usleep(1000);

        XFreeCursor(dpy, cursor);
    }

    message = load_ximage_message();
    granted = load_ximage_granted();
    denied = load_ximage_denied();

    if(activated)
        init_graphics();
    else
        toggle_dpms();

    event_loop();

    cleanup();

    return EXIT_SUCCESS;
}

void cleanup()
{
    int n;

    inputlen = MAX_INPUTLEN;
    while(inputlen-- > 0)
        input[inputlen] = '\0';

    for(n = 0; n < num_screens; n++) {
        XDestroyWindow(dpy, screens[n].win);
        XFreeGC(dpy, screens[n].gc);
    }

    XUngrabPointer(dpy, CurrentTime);

    free(screens);
    free(message);
    free(granted);
    free(denied);

    if(use_dpms)
        DPMSSetTimeouts(dpy, dpms_standby, dpms_suspend, dpms_off);

    XCloseDisplay(dpy);
}

void exit_error(const char *error_str, ...)
{
    va_list ap;
    va_start(ap, error_str);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, error_str, ap);
    va_end(ap);
    cleanup();
    exit(EXIT_FAILURE);
}

void event_loop()
{
    int x11_fd;
    fd_set in_fds;
    struct timeval tv;

    x11_fd = ConnectionNumber(dpy);

    while(1) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);
        tv.tv_usec = 0;
        tv.tv_sec = 60;

        if(!select(x11_fd+1, &in_fds, 0, 0, &tv) && activated)
            clear_graphics();

        while(XPending(dpy))
            if(handle_event())
                return;
    }
}

int handle_event(void)
{
    XEvent ev;
    KeySym key;
    char s[32];
    int n;

    XNextEvent(dpy, &ev);

    if(ev.type == KeyPress) {
        if(!activated) {
            init_graphics();
            return 0;;
        }
        n = XLookupString(&ev.xkey, s, sizeof s, &key, 0);
        if(IsKeypadKey(key)) {
            if(key == XK_KP_Enter)
                key = XK_Return;
            else if(key >= XK_KP_0 && key <= XK_KP_9)
                key = (key - XK_KP_0) + XK_0;
        }
        if(IsFunctionKey(key) || IsKeypadKey(key)
                || IsMiscFunctionKey(key) || IsPFKey(key)
                || IsPrivateKeypadKey(key)) {
            return 0;
        }
        switch(key) {
            case XK_Return:
                if(check_input())
                    return 1;
                break;
            case XK_Escape:
                if(inputlen == 0)
                    clear_graphics();
                inputlen = 0;
                break;
            case XK_BackSpace:
                inputlen = inputlen > 0 ? inputlen - 1 : 0;
                break;
            default:
                if(n && !iscntrl((int)s[0]) &&
                        (n + inputlen < MAX_INPUTLEN)) {
                    memcpy(input + inputlen, s, n);
                    inputlen += n;
                }
                break;
        }
        if(activated)
            draw_input(1);
    } else if(ev.type == Expose && activated) {
        init_graphics();
    } else {
        for(n = 0; n < num_screens; n++)
            XRaiseWindow(dpy, screens[n].win);
    }

    return 0;
}

void toggle_dpms(void)
{
    if(use_dpms) {
        if(activated)
            DPMSSetTimeouts(dpy, 0, 0, 0);
        else
            DPMSSetTimeouts(dpy, 30, 300, 600);
    }
}

void init_graphics(void)
{
    draw_message(0);
    draw_inputfield(0);
    draw_input(1);
    activated = 1;
    toggle_dpms();
}

void clear_graphics(void)
{
    int n;
    for(n = 0; n < num_screens; n++)
        XClearWindow(dpy, screens[n].win);
    activated = 0;
    toggle_dpms();
}

void draw_message(int direct)
{
    int n, x, y;
    for(n = 0; n < num_screens; n++) {
        x = screens[n].x_org + ((screens[n].width * .5) - (message->width * .5));
        y = screens[n].y_org + ((screens[n].height * .5) - (message->height * 2));
        XPutImage(dpy, screens[n].win, screens[n].gc, message, 0, 0, x, y,
                message->width, message->height);
    }

    if(direct)
        update_screens();
}

void draw_inputfield(int direct)
{
    int weight = 3;
    int n, w, h, x, y;

    for(n = 0; n < num_screens; n++) {
        w = screens[n].width - (screens[n].width * .25);
        h = screens[n].height * .05;
        x = screens[n].x_org + ((screens[n].width * .5) - (w * .5));
        y = screens[n].y_org + ((screens[n].height * .5) - (h * .5));
        XSetForeground(dpy, screens[n].gc, fgcolor);
        XFillRectangle(dpy, screens[n].win, screens[n].gc, x, y, w, h);

        screens[n].iw = w - (weight * 2);
        screens[n].ih = h - (weight * 2);
        screens[n].ix = x + weight;
        screens[n].iy = y + weight;
        XSetForeground(dpy, screens[n].gc, bgcolor);
        XFillRectangle(dpy, screens[n].win, screens[n].gc,
                screens[n].ix, screens[n].iy,screens[n].iw, screens[n].ih);
    }

    if(direct)
        update_screens();
}

void draw_access_blank(int direct)
{
    int n, x, y;

    for(n = 0; n < num_screens; n++) {
        x = screens[n].x_org + ((screens[n].width * .5) - (granted->width * .5));
        y = screens[n].y_org + ((screens[n].height * .5) + (granted->width * 2));
        XSetForeground(dpy, screens[n].gc, bgcolor);
        XFillRectangle(dpy, screens[n].win, screens[n].gc, x, y,
                granted->width, granted->height);
    }

    if(direct)
        update_screens();
}

void draw_access(XImage *image, int direct)
{
    int n, x, y;

    draw_access_blank(0);

    for(n = 0; n < num_screens; n++) {
        x = screens[n].x_org + ((screens[n].width * .5) - (image->width * .5));
        y = screens[n].y_org + ((screens[n].height * .5) + (image->height * 2));
        XPutImage(dpy, screens[n].win, screens[n].gc, image, 0, 0, x, y,
                image->width, image->height);
    }

    if(direct)
        update_screens();
}

void draw_input(int direct)
{
    int n, i, len, y, size, step;

    for(n = 0; n < num_screens; n++) {
        XSetForeground(dpy, screens[n].gc, bgcolor);
        XFillRectangle(dpy, screens[n].win, screens[n].gc,
                screens[n].ix, screens[n].iy, screens[n].iw, screens[n].ih);

        XSetForeground(dpy, screens[n].gc, fgcolor);
        len = (inputlen < MAX_WILDCARDS ? inputlen : MAX_WILDCARDS) + 1;

        size = (screens[n].iw / MAX_WILDCARDS) * .5;
        y = screens[n].iy + ((screens[n].ih * .5) - (size * .5));
        step = screens[n].iw / len;

        for(i = 1; i < len; i++)
            XFillRectangle(dpy, screens[n].win, screens[n].gc,
                    screens[n].ix + (step * i), y, size, size);
    }

    if(direct)
        update_screens();
}

void update_screens(void)
{
    XFlush(dpy);
}

int check_input(void)
{
    int access_granted;

    input[inputlen] = '\0';
    draw_access_blank(1);

#ifdef TEST
    access_granted = strcmp(input, "test") == 0;
#else
    access_granted = pam_check_access();
#endif

    while(inputlen)
        input[--inputlen] = '\0';

    if(access_granted) {
        draw_access(granted, 1);
        sleep(1);
    } else {
        draw_access(denied, 1);
    }

    return access_granted;
}

int pam_check_access()
{
    int r;
    pam_handle_t *ph = NULL;
    struct pam_conv pc;;

    pc.conv = &pam_input_conv;
    pc.appdata_ptr = NULL;

    if ((r = pam_start("su", getenv("USER"), &pc, &ph)) != PAM_SUCCESS)
        return 0;

    if ((r = pam_authenticate(ph, PAM_DISALLOW_NULL_AUTHTOK)) != PAM_SUCCESS) {
        pam_end(ph, PAM_DATA_SILENT);
        return 0;
    }

    if ((r = pam_acct_mgmt(ph, 0)) != PAM_SUCCESS) {
        pam_end(ph, PAM_DATA_SILENT);
        return 0;
    }

    return pam_end(ph, PAM_DATA_SILENT) == PAM_SUCCESS;
}

int pam_input_conv(int n, const struct pam_message **msg,
        struct pam_response **resp, void *d)
{
    int i;
    for(i = 0; i < n; i++) {
        if(msg[i]->msg_style != PAM_PROMPT_ECHO_OFF &&
           msg[i]->msg_style != PAM_PROMPT_ECHO_ON) continue;
        resp[i] = calloc(1, sizeof(struct pam_response));
        resp[i]->resp = strdup(input);
    }
    return PAM_SUCCESS;
}
