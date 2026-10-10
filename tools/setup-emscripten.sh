#!/usr/bin/env bash
#
# setup-emscripten.sh DIR -- install an Emscripten toolchain into DIR without
# emsdk's binary mirror: official LLVM release + binaryen release (both from
# GitHub releases) + the matching emscripten git tag.  Writes DIR/env.sh.
#
# If you already use emsdk, you don't need this: just `source emsdk_env.sh`.
#
set -euo pipefail

DIR=${1:?usage: setup-emscripten.sh DIR}
EMSCRIPTEN_TAG=${EMSCRIPTEN_TAG:-6.0.3}      # expects LLVM 23, binaryen 129
LLVM_VER=${LLVM_VER:-23.1.3}
BINARYEN_VER=${BINARYEN_VER:-129}

mkdir -p "$DIR"
DIR=$(cd "$DIR" && pwd)
cd "$DIR"

fetch() {  # url out
  [ -s "$2" ] && return 0
  curl -fL --retry 10 --retry-delay 3 -C - -o "$2.part" "$1"
  mv "$2.part" "$2"
}

LLVM=LLVM-$LLVM_VER-Linux-X64
if [ ! -x "$LLVM/bin/clang" ]; then
  fetch "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VER/$LLVM.tar.xz" llvm.tar.xz
  # only what emscripten needs (the full tarball is ~2 GB unpacked)
  tar -xJf llvm.tar.xz --wildcards \
    "$LLVM/bin/clang" "$LLVM/bin/clang-[0-9]*" "$LLVM/bin/clang++" \
    "$LLVM/bin/lld" "$LLVM/bin/wasm-ld" "$LLVM/bin/llvm-ar" \
    "$LLVM/bin/llvm-ranlib" "$LLVM/bin/llvm-nm" "$LLVM/bin/llvm-objcopy" \
    "$LLVM/bin/llvm-strip" "$LLVM/bin/llvm-dwarfdump" "$LLVM/bin/llvm-dwp" \
    "$LLVM/bin/llvm-symbolizer" "$LLVM/lib/clang/*"
  rm -f llvm.tar.xz
fi

BIN=binaryen-version_$BINARYEN_VER
if [ ! -x "$BIN/bin/wasm-opt" ]; then
  fetch "https://github.com/WebAssembly/binaryen/releases/download/version_$BINARYEN_VER/$BIN-x86_64-linux.tar.gz" binaryen.tar.gz
  tar -xzf binaryen.tar.gz && rm -f binaryen.tar.gz
fi

if [ ! -f emscripten/emscripten-version.txt ]; then
  rm -rf emscripten
  git clone --depth 1 --branch "$EMSCRIPTEN_TAG" https://github.com/emscripten-core/emscripten.git
  # Without .git, emcc treats the tree like a release (no bootstrap.py
  # checks for test submodules and git hooks); we do the needed steps below.
  rm -rf emscripten/.git
fi
cat >emscripten.config <<EOF
LLVM_ROOT = '$DIR/$LLVM/bin'
BINARYEN_ROOT = '$DIR/$BIN'
NODE_JS = '$(command -v node)'
EOF
( cd emscripten && npm ci --omit=dev --no-audit --no-fund >/dev/null \
  && python3 tools/maint/create_entry_points.py )

cat >env.sh <<EOF
export EM_CONFIG='$DIR/emscripten.config'
export PATH='$DIR/emscripten':\$PATH
EOF
. ./env.sh
emcc --version | head -1
