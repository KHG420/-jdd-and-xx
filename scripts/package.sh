#!/bin/zsh
set -euo pipefail
cd "${0:A:h:h}"
[[ -f build/windows/DesktopPets.exe ]] || { echo '请先运行 scripts/build-windows.sh' >&2; exit 1; }
[[ -d build/桌边的你们.app ]] || { echo '请先运行 scripts/build.sh' >&2; exit 1; }
mkdir -p build/releases build/package-windows/windows
cp build/windows/DesktopPets.exe build/package-windows/windows/DesktopPets.exe
cp docs/Windows使用说明.txt build/package-windows/windows/README.txt
ditto -c -k --norsrc --noextattr --noqtn --keepParent build/package-windows/windows build/releases/桌边的你们-Windows-x64.zip
ditto -c -k --sequesterRsrc --keepParent build/桌边的你们.app build/releases/桌边的你们-macOS-universal.zip
shasum -a 256 build/releases/*.zip > build/releases/SHA256SUMS.txt
echo "发布文件：$PWD/build/releases"
