/*
 * keymap.c -- keyboard glue between x11.wasm and Xt/Motif.
 *
 * 1. A fixed keyboard map.  x11.wasm learns keycodes lazily (the browser's
 *    legacy keyCode, remembered on first press) and answers
 *    XKeysymToKeycode() for keys not yet seen with keysym & 0xff, which
 *    collides with letter keycodes (XK_Right 0xff53 -> 83 = 'S',
 *    XK_End -> 'W', ...).  Xt and Motif build their key tables at startup,
 *    so the map must be complete up front: install_browser_keymap() makes
 *    every keycode the browser produces explicit (XChangeKeyboardMapping).
 *
 * 2. Motif's vendor ("osf") keysyms.  libX11 resolves names like
 *    "osfActivate" through the XKeysymDB file; x11.wasm has none, so every
 *    Motif translation table using them failed to parse.  The wrappers
 *    (-Wl,--wrap=XStringToKeysym,--wrap=XKeysymToString) answer the osf
 *    names from <Xm/VirtKeys.h> and defer everything else to x11.wasm.
 */
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <Xm/VirtKeys.h>

/* browser KeyboardEvent.keyCode -> { unshifted, shifted } */
static const struct { unsigned char code; KeySym sym[2]; } keymap[] = {
    {   8, { XK_BackSpace } },        {   9, { XK_Tab, XK_ISO_Left_Tab } },
    {  12, { XK_Clear } },            {  13, { XK_Return } },
    {  16, { XK_Shift_L } },          {  17, { XK_Control_L } },
    {  18, { XK_Alt_L } },            {  19, { XK_Pause } },
    {  20, { XK_Caps_Lock } },        {  27, { XK_Escape } },
    {  32, { XK_space } },            {  33, { XK_Prior } },
    {  34, { XK_Next } },             {  35, { XK_End } },
    {  36, { XK_Home } },             {  37, { XK_Left } },
    {  38, { XK_Up } },               {  39, { XK_Right } },
    {  40, { XK_Down } },             {  44, { XK_Print } },
    {  45, { XK_Insert } },           {  46, { XK_Delete } },
    {  48, { XK_0, XK_parenright } }, {  49, { XK_1, XK_exclam } },
    {  50, { XK_2, XK_at } },         {  51, { XK_3, XK_numbersign } },
    {  52, { XK_4, XK_dollar } },     {  53, { XK_5, XK_percent } },
    {  54, { XK_6, XK_asciicircum } },{  55, { XK_7, XK_ampersand } },
    {  56, { XK_8, XK_asterisk } },   {  57, { XK_9, XK_parenleft } },
    {  59, { XK_semicolon, XK_colon } }, {  61, { XK_equal, XK_plus } },
    {  91, { XK_Meta_L } },           {  92, { XK_Meta_R } },
    {  93, { XK_Menu } },
    {  96, { XK_KP_0 } }, {  97, { XK_KP_1 } }, {  98, { XK_KP_2 } },
    {  99, { XK_KP_3 } }, { 100, { XK_KP_4 } }, { 101, { XK_KP_5 } },
    { 102, { XK_KP_6 } }, { 103, { XK_KP_7 } }, { 104, { XK_KP_8 } },
    { 105, { XK_KP_9 } }, { 106, { XK_KP_Multiply } }, { 107, { XK_KP_Add } },
    { 109, { XK_KP_Subtract } }, { 110, { XK_KP_Decimal } },
    { 111, { XK_KP_Divide } },
    { 112, { XK_F1 } }, { 113, { XK_F2 } }, { 114, { XK_F3 } },
    { 115, { XK_F4 } }, { 116, { XK_F5 } }, { 117, { XK_F6 } },
    { 118, { XK_F7 } }, { 119, { XK_F8 } }, { 120, { XK_F9 } },
    { 121, { XK_F10 } }, { 122, { XK_F11 } }, { 123, { XK_F12 } },
    { 144, { XK_Num_Lock } },         { 145, { XK_Scroll_Lock } },
    { 161, { XK_Shift_R } },          { 163, { XK_Control_R } },
    { 165, { XK_Alt_R } },            { 173, { XK_minus, XK_underscore } },
    { 186, { XK_semicolon, XK_colon } }, { 187, { XK_equal, XK_plus } },
    { 188, { XK_comma, XK_less } },   { 189, { XK_minus, XK_underscore } },
    { 190, { XK_period, XK_greater } }, { 191, { XK_slash, XK_question } },
    { 192, { XK_grave, XK_asciitilde } },
    { 219, { XK_bracketleft, XK_braceleft } },
    { 220, { XK_backslash, XK_bar } },
    { 221, { XK_bracketright, XK_braceright } },
    { 222, { XK_apostrophe, XK_quotedbl } },
    { 225, { XK_Mode_switch } },
};

void
install_browser_keymap(Display *d)
{
    KeySym syms[2];
    size_t i;
    int c;

    for (c = 'A'; c <= 'Z'; c++) {
	syms[0] = XK_a + (c - 'A');
	syms[1] = XK_A + (c - 'A');
	XChangeKeyboardMapping(d, c, 2, syms, 1);
    }
    for (i = 0; i < sizeof keymap / sizeof keymap[0]; i++) {
	/* x11.wasm's lookup does not fall back from an empty shifted
	 * column to the unshifted one, so fill it in. */
	syms[0] = keymap[i].sym[0];
	syms[1] = keymap[i].sym[1] != NoSymbol ? keymap[i].sym[1] : syms[0];
	XChangeKeyboardMapping(d, keymap[i].code, 2, syms, 1);
    }
    /* Nobody has seen the old map yet: drop the MappingNotify events
     * (one per call) instead of letting them fill the event queue. */
    {
	XEvent ev;
	while (XCheckTypedEvent(d, MappingNotify, &ev)) {
	}
    }
}

#define K(n) { "osf" #n, osfXK_##n },
static const struct { const char *name; KeySym sym; } osf[] = {
    K(Activate) K(AddMode) K(BackSpace) K(BackTab) K(BeginData)
    K(BeginLine) K(Cancel) K(Clear) K(Copy) K(Cut) K(Delete)
    K(DeselectAll) K(Down) K(EndData) K(EndLine) K(Escape) K(Extend)
    K(Help) K(Insert) K(Left) K(LeftLine) K(Menu) K(MenuBar) K(Next)
    K(NextField) K(NextMenu) K(NextMinor) K(PageDown) K(PageLeft)
    K(PageRight) K(PageUp) K(Paste) K(PrevField) K(PrevMenu)
    K(PrimaryPaste) K(Prior) K(PriorMinor) K(QuickPaste) K(Reselect)
    K(Restore) K(Right) K(RightLine) K(Select) K(SelectAll)
    K(SwitchDirection) K(Undo) K(Up)
};
#undef K

KeySym __real_XStringToKeysym(const char *);
char *__real_XKeysymToString(KeySym);

KeySym
__wrap_XStringToKeysym(const char *s)
{
    if (s != NULL && strncmp(s, "osf", 3) == 0) {
	size_t i;
	for (i = 0; i < sizeof osf / sizeof osf[0]; i++) {
	    if (strcmp(s, osf[i].name) == 0) {
		return osf[i].sym;
	    }
	}
    }
    return __real_XStringToKeysym(s);
}

char *
__wrap_XKeysymToString(KeySym k)
{
    if ((k & 0xFFFF0000UL) == 0x10040000UL) {
	size_t i;
	for (i = 0; i < sizeof osf / sizeof osf[0]; i++) {
	    if (osf[i].sym == k) {
		return (char *)osf[i].name;
	    }
	}
    }
    return __real_XKeysymToString(k);
}
