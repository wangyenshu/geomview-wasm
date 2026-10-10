/*
 * Minimal SHAPE extension client header for x11.wasm.
 * The browser display has no SHAPE extension: XShapeQueryExtension()
 * reports False and the request functions are no-ops (x11compat.c).
 */
#ifndef _X11_EXTENSIONS_SHAPE_H_
#define _X11_EXTENSIONS_SHAPE_H_
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shapeconst.h>
_XFUNCPROTOBEGIN
extern Bool XShapeQueryExtension(Display *, int *, int *);
extern Status XShapeQueryVersion(Display *, int *, int *);
extern void XShapeCombineRegion(Display *, Window, int, int, int, Region, int);
extern void XShapeCombineRectangles(Display *, Window, int, int, int,
        XRectangle *, int, int, int);
extern void XShapeCombineMask(Display *, Window, int, int, int, Pixmap, int);
extern void XShapeCombineShape(Display *, Window, int, int, int, Window, int, int);
extern void XShapeOffsetShape(Display *, Window, int, int, int);
extern void XShapeSelectInput(Display *, Window, unsigned long);
_XFUNCPROTOEND
#endif
