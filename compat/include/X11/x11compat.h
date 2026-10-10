/*
 * x11compat.h -- prototypes for Xlib entry points that x11.wasm does not
 * declare/implement (definitions in x11compat.c).  Force-included into
 * libXt and Motif (-include X11/x11compat.h) so the calls get correct
 * (and, for the varargs XOM/XOC calls, variadic) signatures.
 */
#ifndef _X11_X11COMPAT_H_
#define _X11_X11COMPAT_H_
#include <X11/Xlib.h>
_XFUNCPROTOBEGIN
extern Status XAddConnectionWatch(Display *, XConnectionWatchProc, XPointer);
extern void XRemoveConnectionWatch(Display *, XConnectionWatchProc, XPointer);
extern void XProcessInternalConnection(Display *, int);
extern Status XInternalConnectionNumbers(Display *, int **, int *);
extern XOM XOMOfOC(XOC);
extern char *XGetOMValues(XOM, ...);
extern char *XGetOCValues(XOC, ...);
extern char *XSetOCValues(XOC, ...);
_XFUNCPROTOEND
#endif
