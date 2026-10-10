# Geomview for WebAssembly

Builds the Geomview Motif GUI (`gvx`) for the browser on x11.wasm + libXt + Motif 2.3.8.

## Build

```
./build.sh
```

Output: `dist/geomview.{html,js,wasm,data}`.

Stages: `fetch x11wasm headers xt motif geomview link`. Run one with `./build.sh <stage>`. `./build.sh clean` removes the build tree.

Variables: `WORK` (build dir), `OUT` (output dir), `JOBS`, `OPT` (default `-O2`), `ASYNC=asyncify|jspi` (default `asyncify`), `LINK_EXTRA` (extra final-link flags).

Requirements:
- `emcc` on `PATH`. If missing, `tools/setup-emscripten.sh` installs Emscripten 6.0.3, LLVM 23.1.3 and Binaryen 129.
- Host tools: `cc make ninja autoconf automake libtool pkg-config flex bison patch git curl python3 node`.
- Network: archive.ubuntu.com, github.com.

## Run

```
cd dist && python3 -m http.server 8000
```

Open `http://localhost:8000/geomview.html`. Pass arguments as `?arg=...&arg=...`, e.g. `geomview.html?arg=/usr/share/geomview/geom/hdodec.off`.

Data files are at `/usr/share/geomview`; the working directory is `/usr/share/geomview/geom`.

## Patches

| File | Change |
|------|--------|
| `geomview-configure-cross.patch` | Cross-compile default for the popen "rb" `AC_RUN_IFELSE` |
| `geomview-discgrp-prototypes.patch` | Fix `void` functions declared as returning pointers in `discgrp/extern.h` |
| `geomview-mg-variadic-prototypes.patch` | Variadic prototypes for `mg_ctxcreate`, `mg_ctxset`, `mgwinchfunc` |
| `x11wasm-event-queue-limit.patch` | Make the x11.wasm event queue size configurable (built with 8192) |

libXt and Motif are unpatched.

## compat/

| File | Purpose |
|------|---------|
| `include/X11/*` | Headers missing from x11.wasm; `x11compat.h` is force-included into Xt and Motif |
| `src/x11compat.c` | SHAPE, MIT-SHM, XOM/XOC, connection-watch and XAddExtension stubs |
| `src/smstubs.c` | libSM stubs |
| `src/wasmwait.c` | `poll`/`select` wrappers that yield to the browser via `emscripten_sleep` |
| `src/keymap.c` | Fixed keymap installed at `XOpenDisplay`; Motif `osf*` keysyms |
| `js/textshift.js` | Adds ShiftMask to shifted characters in x11.wasm text events |
| `js/geomview-env.js` | Geomview environment variables and working directory |

Link flags: `-sEMULATE_FUNCTION_POINTER_CASTS`, `-sASYNCIFY` (or `-sJSPI`).

## Test

```
cd tests
npm install playwright && npx playwright install chromium
node smoke.mjs ../dist
```

Set `CHROME=/path/to/chrome` to use another browser. Screenshots are written to `tests/out/`.

## Limitations

- No window manager: windows cannot be moved or resized.
- No external modules, Help, gzip-compressed textures, or anything that forks.
- Software rendering only (no OpenGL).
- `(backcolor ...)` has no effect (upstream behavior).