/*
 * Minimal X11/Xlibint.h for x11.wasm.
 * x11.wasm has no wire protocol, so only the extension hooks that Motif's
 * ColorObj.c touches are declared here (implemented in x11compat.c).
 */
#ifndef _X11_XLIBINT_H_
#define _X11_XLIBINT_H_
#include <X11/Xlib.h>
_XFUNCPROTOBEGIN
extern XExtCodes *XAddExtension(Display *dpy);
extern int (*XESetCloseDisplay(Display *dpy, int extension,
        int (*proc)(Display *, XExtCodes *)))(Display *, XExtCodes *);
_XFUNCPROTOEND
#endif
