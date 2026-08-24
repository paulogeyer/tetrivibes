#!/bin/sh
set -e
cd "$(dirname "$0")"

windows=0
for arg in "$@"; do
    case "$arg" in
        --windows|windows) windows=1 ;;
        -h|--help)
            echo "usage: $0 [--windows]"
            exit 0
            ;;
        *)
            echo "usage: $0 [--windows]" >&2
            exit 1
            ;;
    esac
done

if [ "$windows" -eq 0 ]; then
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    exit 0
fi

qt="${QT_MINGW:-$HOME/qt/5.15.2/mingw81_64}"
if [ ! -f "$qt/lib/cmake/Qt5/Qt5Config.cmake" ]; then
    echo "Windows Qt 5 not found at $qt (set QT_MINGW)" >&2
    exit 1
fi

cmake -S . -B build-win \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH="$qt" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_PREFIX_PATH="$qt" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j

out=build-win/dist/tetrivibes
mkdir -p "$out/platforms"
cp build-win/tetrivibes.exe "$out/"
cp "$qt/bin/Qt5Core.dll" "$qt/bin/Qt5Gui.dll" "$qt/bin/Qt5Widgets.dll" "$qt/bin/Qt5Network.dll" "$out/"
cp "$qt/plugins/platforms/qwindows.dll" "$out/platforms/"
for f in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libEGL.dll libGLESv2.dll; do
    if [ -f "$qt/bin/$f" ]; then
        cp "$qt/bin/$f" "$out/"
    fi
done
echo "Windows build: $out/tetrivibes.exe"
