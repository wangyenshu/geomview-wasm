/*
 * x11compat.c -- Xlib entry points that x11.wasm does not provide but
 * libXt, Motif or Geomview reference.  Archived into libx11wasm.a.
 *   - SHAPE extension: reported absent, requests are no-ops (Motif DnD)
 *   - XAddExtension/XESetCloseDisplay: Motif ColorObj.c's close hook
 *   - connection watches / internal connections: none exist (libXt)
 *   - XOM/XOC values: only left-to-right text (Motif TextOut.c)
 *   - MIT-SHM: reported absent (Geomview's mgx11, built with -DNO_SHM)
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>
#include <X11/extensions/shape.h>

static XExtCodes fakeCodes;

XExtCodes *
XAddExtension(Display *dpy)
{
    (void)dpy;
    fakeCodes.extension++;
    return &fakeCodes;
}

int (*XESetCloseDisplay(Display *dpy, int extension,
	int (*proc)(Display *, XExtCodes *)))(Display *, XExtCodes *)
{
    (void)dpy; (void)extension; (void)proc;
    return NULL;
}

Bool XShapeQueryExtension(Display *d, int *ev, int *er)
{
    (void)d;
    if (ev) *ev = 0;
    if (er) *er = 0;
    return False;
}

Status XShapeQueryVersion(Display *d, int *maj, int *min)
{
    (void)d;
    if (maj) *maj = 0;
    if (min) *min = 0;
    return 0;
}

void XShapeCombineRegion(Display *d, Window w, int k, int x, int y,
	Region r, int op) { }
void XShapeCombineRectangles(Display *d, Window w, int k, int x, int y,
	XRectangle *r, int n, int op, int ord) { }
void XShapeCombineMask(Display *d, Window w, int k, int x, int y,
	Pixmap p, int op) { }
void XShapeCombineShape(Display *d, Window w, int k, int x, int y,
	Window s, int sk, int op) { }
void XShapeOffsetShape(Display *d, Window w, int k, int x, int y) { }
void XShapeSelectInput(Display *d, Window w, unsigned long m) { }

/* x11.wasm has no internal (ICE/XIM) connections. */
#include <X11/x11compat.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

Status XAddConnectionWatch(Display *d, XConnectionWatchProc p, XPointer c)
	{ return 1; }
void XRemoveConnectionWatch(Display *d, XConnectionWatchProc p, XPointer c) { }
void XProcessInternalConnection(Display *d, int fd) { }
Status XInternalConnectionNumbers(Display *d, int **fds, int *n)
{
    if (fds) *fds = NULL;
    if (n) *n = 0;
    return 1;
}

/* Output methods: only the default left-to-right orientation exists. */
XOM XOMOfOC(XOC oc) { return NULL; }

char *XGetOMValues(XOM om, ...)
{
    va_list ap;
    char *name;

    va_start(ap, om);
    name = va_arg(ap, char *);
    va_end(ap);
    return name ? name : "";
}

char *XGetOCValues(XOC oc, ...)
{
    va_list ap;
    char *name;

    va_start(ap, oc);
    while ((name = va_arg(ap, char *)) != NULL) {
	if (strcmp(name, XNOrientation) == 0) {
	    *va_arg(ap, XOrientation *) = XOMOrientation_LTR_TTB;
	} else {
	    break;
	}
    }
    va_end(ap);
    return name;
}

char *XSetOCValues(XOC oc, ...)
{
    va_list ap;
    char *name;

    va_start(ap, oc);
    name = va_arg(ap, char *);
    va_end(ap);
    return name;
}

/*
 * MIT-SHM: not available (no shared memory between page and "server").
 * Geomview is built with -DNO_SHM, but still references XShmPutImage in a
 * branch that only runs when XShmQueryExtension() said yes.  Prototypes
 * follow <X11/extensions/XShm.h>; the segment info is opaque here.
 */
Bool XShmQueryExtension(Display *d) { return False; }
Bool XShmQueryVersion(Display *d, int *maj, int *min, Bool *pixmaps)
{
    if (maj) *maj = 0;
    if (min) *min = 0;
    if (pixmaps) *pixmaps = False;
    return False;
}
int XShmPixmapFormat(Display *d) { return 0; }
Bool XShmAttach(Display *d, void *shminfo) { return False; }
Bool XShmDetach(Display *d, void *shminfo) { return False; }
Bool XShmPutImage(Display *d, Drawable dr, GC gc, XImage *img, int sx, int sy,
	int dx, int dy, unsigned int w, unsigned int h, Bool send_event)
	{ return False; }
Bool XShmGetImage(Display *d, Drawable dr, XImage *img, int x, int y,
	unsigned long plane_mask) { return False; }
XImage *XShmCreateImage(Display *d, Visual *v, unsigned int depth, int format,
	char *data, void *shminfo, unsigned int w, unsigned int h)
	{ return NULL; }
Pixmap XShmCreatePixmap(Display *d, Drawable dr, char *data, void *shminfo,
	unsigned int w, unsigned int h, unsigned int depth) { return None; }
