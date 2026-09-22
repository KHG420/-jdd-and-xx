#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
if [[ ! -x "build/桌边的你们.app/Contents/MacOS/DesktopPets" ]] || [[ "${1:-}" == "--rebuild" ]]; then
    /bin/zsh scripts/build.sh
fi
open "build/桌边的你们.app"
