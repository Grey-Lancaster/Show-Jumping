#!/bin/bash
# Web demo build (Emscripten): src/main.cpp + src/audio.cpp unchanged, with the browser board layer in web/.
# Output: docs/demo/showjump.js + showjump.wasm. Run from the project root: web/build.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p docs/demo
em++ -O3 -std=gnu++17 -fno-exceptions -Wno-unused-function -Wno-c++11-narrowing \
  -I web/shim -I web -I src \
  src/main.cpp src/audio.cpp web/web_main.cpp web/favicon_stub.cpp \
  -o docs/demo/showjump.js \
  -sMODULARIZE=1 -sEXPORT_NAME=ShowJump -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToNewUTF8,HEAPU16,HEAP16 \
  -sEXPORTED_FUNCTIONS=_demo_init,_demo_frame,_demo_pixels,_demo_width,_demo_height,_demo_touch,_demo_audio,_demo_api,_free
ls -l docs/demo/showjump.js docs/demo/showjump.wasm
