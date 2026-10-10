#!/usr/bin/env bash
#
# build.sh -- build the Geomview Motif GUI (gvx) for the browser.
#
# Chain:  x11.wasm (Xlib for the browser)  ->  libXt  ->  Motif libXm
#         ->  Geomview (mgx11 software renderer, Motif panels)
#
# Usage:  ./build.sh [stage...]
#   stages: fetch x11wasm headers xt motif geomview link   (default: all)
#   ./build.sh clean        remove the build tree (keeps downloads)
#
# Environment knobs:
#   WORK=dir        build tree               (default: ./build)
#   OUT=dir         final web files          (default: ./dist)
#   JOBS=n          parallel make jobs       (default: nproc)
#   ASYNC=asyncify|jspi   how the blocking X event loop yields to the
#                   browser (default: asyncify, works in every browser;
#                   jspi has less overhead but needs a JSPI-capable browser)
#   OPT=-O2         optimisation level for everything
#   LINK_EXTRA=...  extra emcc flags for the final link (e.g. --profiling-funcs)
#   GEOMVIEW_REPO / GEOMVIEW_REF, X11WASM_REPO / X11WASM_REF
#
# Needs: emcc on PATH (emsdk; any recent 4.x-6.x) -- if missing, this script
# runs tools/setup-emscripten.sh --, plus host tools: cc, make, ninja,
# autoconf, automake, libtool, pkg-config, flex, bison, patch, git, curl,
# python3, node.  Network access: archive.ubuntu.com, github.com (sources,
# and emcc's zlib port).

set -euo pipefail

ROOT=$(cd "$(dirname "$0")" && pwd)
WORK=${WORK:-$ROOT/build}
OUT=${OUT:-$ROOT/dist}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
ASYNC=${ASYNC:-asyncify}

DL=$WORK/downloads
SRC=$WORK/src
BLD=$WORK/obj
PREFIX=$WORK/prefix
STAMPS=$WORK/stamps

GEOMVIEW_REPO=${GEOMVIEW_REPO:-https://github.com/geomview/geomview.git}
GEOMVIEW_REF=${GEOMVIEW_REF:-1d6759ec9771fbc6bdc71ec79b7a5a28711cd9fd}
X11WASM_REPO=${X11WASM_REPO:-https://github.com/t3tra-dev/x11.wasm.git}
X11WASM_REF=${X11WASM_REF:-122b98a65c465a354a51fe0f5214406da8930f9a}

# name|sha256|url [url...]   (Ubuntu's *.orig.tar.gz are the upstream tarballs)
UBU=https://archive.ubuntu.com/ubuntu/pool
TARBALLS=(
  "xorgproto-2024.1|4f6b9b4faf91e5df8265b71843a91fc73dc895be6210c84117a996545df296ce|$UBU/main/x/xorgproto/xorgproto_2024.1.orig.tar.gz https://www.x.org/releases/individual/proto/xorgproto-2024.1.tar.gz"
  "libICE-1.0.10|1116bc64c772fd127a0d0c0ffa2833479905e3d3d8197740b3abd5f292f22d2d|$UBU/main/libi/libice/libice_1.0.10.orig.tar.gz"
  "libSM-1.2.3|1e92408417cb6c6c477a8a6104291001a40b3bb56a4a60608fdd9cd2c5a0f320|$UBU/main/libs/libsm/libsm_1.2.3.orig.tar.gz"
  "libXt-1.2.1|6da1bfa9dd0ed87430a5ce95b129485086394df308998ebe34d98e378e3dfb33|$UBU/main/libx/libxt/libxt_1.2.1.orig.tar.gz https://www.x.org/releases/individual/lib/libXt-1.2.1.tar.gz"
  "motif-2.3.8|859b723666eeac7df018209d66045c9853b50b4218cecadb794e2359619ebce7|$UBU/universe/m/motif/motif_2.3.8.orig.tar.gz https://downloads.sourceforge.net/project/motif/Motif%202.3.8%20Source%20Code/motif-2.3.8.tar.gz"
)

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }
stamp() { mkdir -p "$STAMPS"; touch "$STAMPS/$1"; }
done_() { [ -e "$STAMPS/$1" ]; }

# ---------------------------------------------------------------- toolchain
setup_emcc() {
  if ! command -v emcc >/dev/null 2>&1; then
    if [ -f "$WORK/toolchain/env.sh" ] || "$ROOT/tools/setup-emscripten.sh" "$WORK/toolchain"; then
      # shellcheck disable=SC1091
      . "$WORK/toolchain/env.sh"
    fi
  fi
  command -v emcc >/dev/null 2>&1 || die "emcc not found (activate emsdk or run tools/setup-emscripten.sh)"
  export EM_CACHE=${EM_CACHE:-$WORK/em-cache}
  BUILD_TRIPLET=$(cc -dumpmachine 2>/dev/null || echo x86_64-pc-linux-gnu)
  HOST_TRIPLET=wasm32-unknown-emscripten
  # Old K&R-era sources: keep clang's newer hard errors as warnings.
  OLDC="-Wno-error=implicit-function-declaration -Wno-error=implicit-int \
-Wno-error=incompatible-function-pointer-types -Wno-error=int-conversion \
-Wno-error=incompatible-pointer-types -Wno-error=return-type"
  OPT=${OPT:--O2}
  # emconfigure/emmake take extra pkg-config dirs from EM_PKG_CONFIG_PATH
  export EM_PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
}

# -------------------------------------------------------------------- fetch
fetch_tarball() {   # name sha urls...
  local name=$1 sha=$2; shift 2
  local f=$DL/$name.tar.gz
  if [ -f "$f" ] && echo "$sha  $f" | sha256sum -c --status; then return; fi
  for u in "$@"; do
    log "download $name ($u)"
    if curl -fL --retry 5 --retry-delay 2 -C - -o "$f.part" "$u"; then
      if echo "$sha  $f.part" | sha256sum -c --status; then mv "$f.part" "$f"; return; fi
      echo "checksum mismatch for $u" >&2
    fi
    rm -f "$f.part"
  done
  die "could not fetch $name"
}

git_checkout() {    # dir repo ref
  local dir=$1 repo=$2 ref=$3
  if [ ! -d "$dir/.git" ]; then git clone --filter=blob:none "$repo" "$dir"; fi
  git -C "$dir" fetch -q origin "$ref" 2>/dev/null || true
  git -C "$dir" checkout -q -f "$ref"
}

stage_fetch() {
  mkdir -p "$DL" "$SRC"
  for t in "${TARBALLS[@]}"; do
    IFS='|' read -r name sha urls <<<"$t"
    # shellcheck disable=SC2086
    fetch_tarball "$name" "$sha" $urls
    [ -d "$SRC/$name" ] || tar -xzf "$DL/$name.tar.gz" -C "$SRC"
  done
  git_checkout "$SRC/x11.wasm" "$X11WASM_REPO" "$X11WASM_REF"
  git_checkout "$SRC/geomview" "$GEOMVIEW_REPO" "$GEOMVIEW_REF"
  stamp fetch
}

# ------------------------------------------------------------------ x11wasm
apply_patches() {   # srcdir prefix
  local p
  for p in "$ROOT"/patches/"$2"-*.patch; do
    [ -e "$p" ] || continue
    if patch -d "$1" -p1 -R --dry-run -s -f <"$p" >/dev/null 2>&1; then continue; fi
    log "apply $(basename "$p")"; patch -d "$1" -p1 -f <"$p"
  done
}

stage_x11wasm() {
  log "build x11.wasm"
  apply_patches "$SRC/x11.wasm" x11wasm
  # Geomview builds ~100 windows before its first event-loop pass; x11.wasm
  # drops events once its queue (256 by default) is full.
  rm -rf "$SRC/x11.wasm/build/emscripten"
  ( cd "$SRC/x11.wasm" && \
    EMCC_CFLAGS="$OPT -DX11_WASM_EVENT_QUEUE_LIMIT=8192" ninja -f build.ninja x11wasm )
  stamp x11wasm
}

# ------------------------------------------------------------------ headers
# One include tree: xorgproto + ICE/SM headers (Xt's ShellP.h needs the
# types), then x11.wasm's own Xlib headers on top, then our small shims.
stage_headers() {
  log "assemble X11 headers + compat library"
  local inc=$PREFIX/include lib=$PREFIX/lib x=$SRC/x11.wasm
  mkdir -p "$inc/X11" "$lib/pkgconfig"
  cp -R "$SRC/xorgproto-2024.1/include/X11/." "$inc/X11/"
  rm -f "$inc/X11/meson.build" "$inc/X11/Xpoll.h.in"
  sed 's/@USE_FDS_BITS@/fds_bits/' "$SRC/xorgproto-2024.1/include/X11/Xpoll.h.in" >"$inc/X11/Xpoll.h"
  mkdir -p "$inc/X11/ICE" "$inc/X11/SM"
  cp "$SRC"/libICE-1.0.10/include/X11/ICE/*.h "$inc/X11/ICE/"
  cp "$SRC"/libSM-1.2.3/include/X11/SM/*.h "$inc/X11/SM/"
  cp -R "$x/include/." "$inc/"
  cp -R "$ROOT/compat/include/." "$inc/"

  # libx11wasm.a = x11.wasm + compat shims.  (Not "libX11.a": emcc maps
  # -lX11 to its own tiny library_xlib.js.)
  mkdir -p "$BLD/compat"
  emcc $OPT -I"$inc" -c "$ROOT/compat/src/x11compat.c" -o "$BLD/compat/x11compat.o"
  cp "$x/build/emscripten/libx11wasm.a" "$lib/libx11wasm.a"
  emar rcs "$lib/libx11wasm.a" "$BLD/compat/x11compat.o"
  # libSM.a: session-management stubs for libXt (no session manager here)
  emcc $OPT -I"$inc" -c "$ROOT/compat/src/smstubs.c" -o "$BLD/compat/smstubs.o"
  rm -f "$lib/libSM.a"; emar rcs "$lib/libSM.a" "$BLD/compat/smstubs.o"
  cp "$x/build/emscripten/js/emscripten/library.js" "$lib/x11wasm-library.js"
  cp "$x/build/emscripten/js/browser/dom.js" "$lib/x11wasm-dom.js"

  local pc n l libs
  for pc in x11:x11wasm xproto: kbproto: ice: sm:SM ; do
    n=${pc%%:*} l=${pc#*:} libs=
    [ -n "$l" ] && libs="-L\${libdir} -l$l"
    cat >"$lib/pkgconfig/$n.pc" <<EOF
prefix=$PREFIX
includedir=\${prefix}/include
libdir=\${prefix}/lib
Name: $n
Description: $n (x11.wasm build)
Version: 99.0
Cflags: -I\${includedir}
Libs: $libs
EOF
  done
  stamp headers
}

# Link flags configure tests need to link against libx11wasm.a.
cfg_ldflags() { echo "-L$PREFIX/lib --js-library $PREFIX/lib/x11wasm-library.js"; }

# ----------------------------------------------------------------------- Xt
stage_xt() {
  log "build libXt"
  local b=$BLD/libXt
  rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && emconfigure "$SRC/libXt-1.2.1/configure" \
      --host=$HOST_TRIPLET --build="$BUILD_TRIPLET" --prefix="$PREFIX" \
      --disable-shared --enable-static --disable-xkb --disable-specs \
      --without-xmlto --without-fop --without-xsltproc --without-perl \
      CC_FOR_BUILD=cc CFLAGS_FOR_BUILD=-O1 xorg_cv_malloc0_returns_null=no \
      CPPFLAGS="-I$PREFIX/include" CFLAGS="$OPT $OLDC -include X11/x11compat.h" \
      LDFLAGS="$(cfg_ldflags)" )
  emmake make -C "$b" -j"$JOBS"
  emmake make -C "$b" install
  stamp xt
}

# -------------------------------------------------------------------- Motif
stage_motif() {
  log "build Motif (libXm)"
  local s=$SRC/motif-2.3.8 b=$BLD/motif
  # Motif 2.3.8 predates the emscripten target triplet.
  cp "$(automake --print-libdir)/config.sub" "$(automake --print-libdir)/config.guess" "$s/"
  rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && emconfigure "$s/configure" \
      --host=$HOST_TRIPLET --build="$BUILD_TRIPLET" --prefix="$PREFIX" \
      --disable-shared --enable-static --x-includes="$PREFIX/include" \
      --x-libraries="$PREFIX/lib" --disable-printing --disable-xft \
      --disable-jpeg --disable-png --disable-message-catalog \
      CPPFLAGS="-I$PREFIX/include" CFLAGS="$OPT $OLDC -include X11/x11compat.h" \
      LDFLAGS="$(cfg_ldflags)" LIBS="-lSM" \
      ac_cv_file__usr_X_include_X11_X_h=no \
      ac_cv_file__usr_X11R6_include_X11_X_h=no ac_cv_func_setpgrp_void=yes )
  # makestrs generates XmStrDefs.c at build time: it must run on the host.
  cc -O1 -I"$b/include" -I"$b" -I"$PREFIX/include" \
     "$s/config/util/makestrs.c" -o "$b/config/util/makestrs"
  # (the generated string files share one rule: make them serially first)
  emmake make -C "$b/lib/Xm" -j1 XmStrDefs.c
  emmake make -C "$b/lib/Xm" -j"$JOBS"
  emmake make -C "$b/lib/Xm" install
  stamp motif
}

# ----------------------------------------------------------------- geomview
stage_geomview() {
  log "build Geomview"
  local s=$SRC/geomview b=$BLD/geomview
  apply_patches "$s" geomview
  if [ ! -x "$s/configure" ] || [ "$s/configure.ac" -nt "$s/configure" ]; then
    ( cd "$s" && autoreconf -fi )
  fi
  # lisp2c writes clisp.[ch] into the *source* tree, and configure symlinks
  # every header into include/ from wherever it finds it: generate it first.
  [ -f "$s/src/lib/oogl/lisp/clisp.h" ] ||
    ( cd "$s/src/lib/oogl/lisp" && sh ./lisp2c -o clisp lisp.c )
  # zlib comes from emscripten's port (only needed to compile image.c).
  emcc -sUSE_ZLIB=1 -E -x c /dev/null -o /dev/null
  # geomview's configure hardcodes -lXext; the SHAPE stubs live in libx11wasm.
  emar rcs "$PREFIX/lib/libXext.a"
  rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && emconfigure "$s/configure" \
      --host=$HOST_TRIPLET --build="$BUILD_TRIPLET" --prefix=/usr \
      --disable-shared --enable-static --with-motif="$PREFIX" \
      --without-opengl --x-includes="$PREFIX/include" \
      --x-libraries="$PREFIX/lib" --with-zlib-dir="$EM_CACHE/sysroot" \
      CPPFLAGS="-I$PREFIX/include -DNO_SHM" CFLAGS="$OPT $OLDC -sUSE_ZLIB=1" \
      CXXFLAGS="$OPT" LDFLAGS="$(cfg_ldflags) -sUSE_ZLIB=1" \
      LIBS="-lx11wasm -lSM" ac_cv_func_setpgrp_void=yes ac_cv_func_stat_empty_string_bug=no \
      ac_cv_func_lstat_empty_string_bug=no ac_cv_func_strtod=yes \
      ac_cv_func_lstat_dereferences_slashed_symlink=yes \
      ac_cv_func_memcmp_working=yes ac_cv_func_closedir_void=no )
  emmake make -C "$b/src/lib" -j"$JOBS"
  emmake make -C "$b/src/bin/geomview" -j"$JOBS"
  stamp geomview
}

# ----------------------------------------------------------------- link
# Final link of gvx (objects from the geomview build) into a web page.
stage_link() {
  log "link geomview.html"
  local b=$BLD/geomview g=$BLD/geomview/src/bin/geomview/x11 l=$BLD/link
  local async
  mkdir -p "$l" "$OUT"
  local cf="$OPT -I$PREFIX/include"
  emcc $cf -c "$ROOT/compat/src/wasmwait.c" -o "$l/wasmwait.o"
  emcc $cf -c "$ROOT/compat/src/keymap.c" -o "$l/keymap.o"
  case $ASYNC in
    jspi) async="-sJSPI" ;;
    *)    async="-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536" ;;
  esac
  rm -f "$OUT"/geomview.*
  # Xt/Motif/Geomview call many callbacks through mismatched function-pointer
  # types (harmless in native C ABIs, a trap in wasm), hence
  # EMULATE_FUNCTION_POINTER_CASTS; its thunks must cover 29 parameters.
  # The --wrap'ed functions live in compat/src/{wasmwait,keymap}.c.
  emcc $OPT -o "$OUT/geomview.html" \
    "$g"/gv*.o "$g/buildinfo.o" "$l/wasmwait.o" "$l/keymap.o" \
    "$b/src/bin/geomview/common/libgvcommon.a" "$b/src/lib/mib/libmib.a" \
    "$b/src/lib/.libs/libgeomview.a" \
    -L"$PREFIX/lib" -lXm -lXt -lSM -lx11wasm -sUSE_ZLIB=1 -lm \
    -Wl,--wrap=poll,--wrap=select,--wrap=XOpenDisplay \
    -Wl,--wrap=XStringToKeysym,--wrap=XKeysymToString \
    --js-library "$PREFIX/lib/x11wasm-library.js" \
    --pre-js "$PREFIX/lib/x11wasm-dom.js" \
    --pre-js "$ROOT/compat/js/textshift.js" \
    --pre-js "$ROOT/compat/js/geomview-env.js" \
    --preload-file "$SRC/geomview/data@/usr/share/geomview" \
    --exclude-file "*Makefile*" --exclude-file "*.cvsignore" \
    --shell-file "$ROOT/web/shell.html" \
    $async -sEMULATE_FUNCTION_POINTER_CASTS \
    -sBINARYEN_EXTRA_PASSES=--pass-arg=max-func-params@29 \
    -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=64MB \
    -sSTACK_SIZE=2MB -sEXIT_RUNTIME=0 -sERROR_ON_UNDEFINED_SYMBOLS=1 \
    ${LINK_EXTRA:-}
  stamp link
}

# --------------------------------------------------------------------- main
ALL=(fetch x11wasm headers xt motif geomview link)
if [ "${1:-}" = clean ]; then rm -rf "$BLD" "$PREFIX" "$STAMPS" "$SRC"; exit 0; fi
setup_emcc
if [ $# -eq 0 ]; then
  for s in "${ALL[@]}"; do done_ "$s" || "stage_$s"; done
else
  for s in "$@"; do "stage_$s"; done
fi
log "done"
