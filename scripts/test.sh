#!/bin/zsh
set -euo pipefail
cd "${0:A:h:h}"
mkdir -p build
swiftc -Onone Sources/Behavior.swift Sources/Sprites.swift Sources/DesktopPets.swift Tests/main.swift -o build/DesktopPetsTests -framework AppKit -framework QuartzCore -framework ImageIO
build/DesktopPetsTests
