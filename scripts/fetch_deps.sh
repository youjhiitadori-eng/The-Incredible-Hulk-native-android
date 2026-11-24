#!/usr/bin/env bash
# Download SDL2, miniaudio, stb into third_party/ (dipanggil otomatis oleh GitHub Actions)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p third_party
[ -d third_party/SDL ] || git clone --depth 1 --branch release-2.30.9 https://github.com/libsdl-org/SDL.git third_party/SDL
[ -d third_party/miniaudio ] || git clone --depth 1 --branch 0.11.21 https://github.com/mackron/miniaudio.git third_party/miniaudio
[ -d third_party/stb ] || git clone --depth 1 https://github.com/nothings/stb.git third_party/stb
echo "third_party OK"
