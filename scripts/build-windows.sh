#!/bin/zsh
set -euo pipefail
cd "${0:A:h:h}"
command -v x86_64-w64-mingw32-g++ >/dev/null || { echo '需要 MinGW-w64 交叉编译器。macOS 开发机可用 brew install mingw-w64 安装。' >&2; exit 1; }
mkdir -p build/windows
x86_64-w64-mingw32-windres -I . Windows/resources.rc -o build/windows/resources.o
x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -static-libgcc -static-libstdc++ Windows/DesktopPets.cpp build/windows/resources.o -o build/windows/DesktopPets.exe -lgdiplus -lshlwapi -lshell32 -lole32 -lshcore -lpowrprof -luuid -luser32 -lgdi32
x86_64-w64-mingw32-strip build/windows/DesktopPets.exe
echo "已构建：$PWD/build/windows/DesktopPets.exe"
