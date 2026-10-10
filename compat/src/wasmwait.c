/*
 * wasmwait.c -- make blocking waits in Xt and Geomview yield to the browser.
 *
 * poll()/select()/XOpenDisplay() wrappers, linked with
 * -Wl,--wrap=poll,--wrap=select,--wrap=XOpenDisplay.  (The XOpenDisplay
 * wrapper also installs the fixed keyboard map from keymap.c.)
 *
 * x11.wasm has no socket: ConnectionNumber() is a pseudo fd (0).  The
 * wrappers report that fd readable when x11.wasm has queued events, pass
 * every other fd to the real call with a zero timeout, and turn a timed or
 * infinite wait into emscripten_sleep() slices so the browser can deliver
 * DOM events and paint.  Event-loop polls (those that watch the X fd) also
 * yield every YIELD_MS when they would not block, so a busy loop (Geomview
 * animating) never starves the browser.
 *
 * Needs -sASYNCIFY (default) or -sJSPI for emscripten_sleep().
 */
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include <emscripten.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef YIELD_MS
#define YIELD_MS 15.0
#endif
#ifndef SLICE_MS
#define SLICE_MS 8.0
#endif

/* ------------------------------------------------------------ display fd */

extern Display *__real_XOpenDisplay(const char *name);
extern void install_browser_keymap(Display *d);	/* keymap.c */

static Display *gDisplay;
static int gXfd = -1;

Display *
__wrap_XOpenDisplay(const char *name)
{
    Display *d = __real_XOpenDisplay(name);

    if (d != NULL && gDisplay == NULL) {
	gDisplay = d;
	gXfd = ConnectionNumber(d);
	install_browser_keymap(d);
    }
    return d;
}

static int
xready(void)
{
    return gDisplay != NULL && XEventsQueued(gDisplay, QueuedAfterFlush) > 0;
}

/* ---------------------------------------------------------------- yield */

static double lastYield;

static void
yield(double ms)
{
    emscripten_sleep((unsigned int)(ms + 0.5));
    lastYield = emscripten_get_now();
}

static void
maybe_yield(void)
{
    if (emscripten_get_now() - lastYield >= YIELD_MS) {
	yield(0);
    }
}

/* ----------------------------------------------------------------- poll */

extern int __real_poll(struct pollfd *fds, nfds_t n, int timeout);

static int
poll_once(struct pollfd *fds, nfds_t n)
{
    struct pollfd tmp[64];
    nfds_t i, m = 0;
    int ready = 0, xr = -1;

    for (i = 0; i < n; i++) {
	fds[i].revents = 0;
    }
    for (i = 0; i < n; i++) {
	if (fds[i].fd == gXfd && gXfd >= 0) {
	    if (xr < 0) {
		xr = xready();
	    }
	    if (xr && (fds[i].events & POLLIN)) {
		fds[i].revents = POLLIN;
		ready++;
	    }
	} else if (fds[i].fd >= 0 && m < 64) {
	    tmp[m] = fds[i];
	    tmp[m].revents = 0;
	    m++;
	}
    }
    if (m > 0 && __real_poll(tmp, m, 0) > 0) {
	nfds_t j = 0;
	for (i = 0; i < n && j < m; i++) {
	    if (fds[i].fd == gXfd && gXfd >= 0) {
		continue;
	    }
	    if (fds[i].fd < 0) {
		continue;
	    }
	    fds[i].revents = tmp[j++].revents;
	    if (fds[i].revents) {
		ready++;
	    }
	}
    }
    return ready;
}

int
__wrap_poll(struct pollfd *fds, nfds_t n, int timeout)
{
    double deadline = emscripten_get_now() + (timeout > 0 ? timeout : 0);
    int r, isx = 0;
    nfds_t i;

    /* Only event-loop waits (those watching the X connection) yield when
     * they would not block; a plain "is data ready?" check on a file
     * returns at once. */
    for (i = 0; i < n; i++) {
	if (gXfd >= 0 && fds[i].fd == gXfd) {
	    isx = 1;
	}
    }
    for (;;) {
	r = poll_once(fds, n);
	if (r > 0 || timeout == 0) {
	    if (isx) {
		maybe_yield();
	    }
	    return r;
	}
	if (timeout > 0) {
	    double left = deadline - emscripten_get_now();
	    if (left <= 0) {
		return 0;
	    }
	    yield(left < SLICE_MS ? left : SLICE_MS);
	} else {
	    yield(SLICE_MS);
	}
    }
}

/* --------------------------------------------------------------- select */

extern int __real_select(int n, fd_set *r, fd_set *w, fd_set *e,
	struct timeval *tv);

static int
select_once(int n, fd_set *r, fd_set *w, fd_set *e,
	const fd_set *r0, const fd_set *w0, const fd_set *e0)
{
    fd_set rr, ww, ee;
    struct timeval zero = { 0, 0 };
    int xwant = 0, xr = 0, count, rn = n;

    if (r0) rr = *r0; else FD_ZERO(&rr);
    if (w0) ww = *w0; else FD_ZERO(&ww);
    if (e0) ee = *e0; else FD_ZERO(&ee);
    if (gXfd >= 0 && gXfd < n) {
	xwant = FD_ISSET(gXfd, &rr);
	FD_CLR(gXfd, &rr);
	FD_CLR(gXfd, &ww);
	FD_CLR(gXfd, &ee);
	if (xwant) {
	    xr = xready();
	}
    }
    count = 0;
    while (rn > 0 && !FD_ISSET(rn - 1, &rr) && !FD_ISSET(rn - 1, &ww)
	    && !FD_ISSET(rn - 1, &ee)) {
	rn--;
    }
    if (rn > 0) {
	count = __real_select(rn, &rr, &ww, &ee, &zero);
	if (count < 0) {
	    count = 0;
	    FD_ZERO(&rr); FD_ZERO(&ww); FD_ZERO(&ee);
	}
    } else {
	FD_ZERO(&rr); FD_ZERO(&ww); FD_ZERO(&ee);
    }
    if (xr) {
	FD_SET(gXfd, &rr);
	count++;
    }
    if (r) *r = rr;
    if (w) *w = ww;
    if (e) *e = ee;
    return count;
}

int
__wrap_select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
    fd_set r0, w0, e0;
    double timeout = tv ? tv->tv_sec * 1000.0 + tv->tv_usec / 1000.0 : -1;
    double deadline = emscripten_get_now() + (timeout > 0 ? timeout : 0);
    int c, isx;

    if (r) r0 = *r;
    if (w) w0 = *w;
    if (e) e0 = *e;
    isx = r != NULL && gXfd >= 0 && gXfd < n && FD_ISSET(gXfd, r);
    for (;;) {
	c = select_once(n, r, w, e, r ? &r0 : NULL, w ? &w0 : NULL,
		e ? &e0 : NULL);
	if (c > 0 || timeout == 0) {
	    if (isx) {
		maybe_yield();
	    }
	    return c;
	}
	if (timeout > 0) {
	    double left = deadline - emscripten_get_now();
	    if (left <= 0) {
		if (tv) tv->tv_sec = tv->tv_usec = 0;
		return 0;
	    }
	    yield(left < SLICE_MS ? left : SLICE_MS);
	} else {
	    yield(SLICE_MS);
	}
    }
}

