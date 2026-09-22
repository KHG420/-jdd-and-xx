#!/bin/zsh
set -euo pipefail
cd "${0:A:h:h}"
APP="$PWD/build/桌边的你们.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" build/macos-architectures
for target_arch in arm64 x86_64; do
    swiftc -O -target "${target_arch}-apple-macosx13.0" Sources/Behavior.swift Sources/Sprites.swift Sources/DesktopPets.swift Sources/main.swift -o "build/macos-architectures/DesktopPets-$target_arch" -framework AppKit -framework QuartzCore -framework ImageIO
done
lipo -create build/macos-architectures/DesktopPets-arm64 build/macos-architectures/DesktopPets-x86_64 -output "$APP/Contents/MacOS/DesktopPets"
cp Assets/*-atlas.png "$APP/Contents/Resources/"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>local.aq.desktop-companions</string>
<key>CFBundleName</key><string>桌边的你们</string>
<key>CFBundleDisplayName</key><string>桌边的你们</string>
<key>CFBundleExecutable</key><string>DesktopPets</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>1.1.0</string>
<key>CFBundleVersion</key><string>2</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>LSUIElement</key><true/>
<key>NSHighResolutionCapable</key><true/>
<key>NSSupportsAutomaticGraphicsSwitching</key><true/>
</dict></plist>
PLIST
codesign --force --sign - "$APP"
echo "已构建：$APP"
