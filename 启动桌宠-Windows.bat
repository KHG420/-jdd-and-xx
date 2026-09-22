@echo off
setlocal
cd /d "%~dp0"
if not exist "build\windows\DesktopPets.exe" (
  echo Windows application is missing. Use the Windows release ZIP,
  echo or build it with scripts\build-windows.ps1 in a Visual Studio developer shell.
  pause
  exit /b 1
)
start "" "build\windows\DesktopPets.exe"
