#!/bin/bash
# Build the web demo on Greyhound (Emscripten in ~/emsdk there) and bring docs/demo/showjump.js + .wasm back.
set -e
cd "$(dirname "$0")/.."
SSH="ssh -o ControlMaster=no -o ControlPath=none GreyHound"
tar cf - src web | $SSH "mkdir -p ~/showjump-web && cd ~/showjump-web && rm -rf src web && tar xf - && source ~/emsdk/emsdk_env.sh >/dev/null 2>&1 && bash web/build.sh"
$SSH "cd ~/showjump-web/docs/demo && tar cf - showjump.js showjump.wasm" | tar xf - -C docs/demo
ls -l docs/demo/showjump.js docs/demo/showjump.wasm
