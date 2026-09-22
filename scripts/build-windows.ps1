$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
New-Item -ItemType Directory -Force build/windows | Out-Null
# Run in an x64 Native Tools Command Prompt / Developer PowerShell for VS 2022.
# These are build tools only. The resulting exe needs no redistributable installer.
& rc.exe /nologo /I . /fo build/windows/resources.res Windows/resources.rc
if ($LASTEXITCODE -ne 0) { throw 'Windows resource compilation failed' }
& cl.exe /nologo /std:c++17 /O2 /EHsc /MT /utf-8 /D_WIN32_WINNT=0x0A00 /Fo:build/windows/DesktopPets.obj /Fe:build/windows/DesktopPets.exe Windows/DesktopPets.cpp /link build/windows/resources.res /SUBSYSTEM:WINDOWS gdiplus.lib shlwapi.lib shell32.lib ole32.lib shcore.lib powrprof.lib uuid.lib user32.lib gdi32.lib
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
Write-Host "已构建：$PWD/build/windows/DesktopPets.exe"
