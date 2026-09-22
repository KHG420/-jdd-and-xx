$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
New-Item -ItemType Directory -Force build/windows-verification | Out-Null
& cl.exe /nologo /std:c++17 /EHsc /MT /utf-8 /Fo:build/windows/BehaviorTests.obj /Fe:build/windows/BehaviorTests.exe Tests/windows-behavior.cpp
if ($LASTEXITCODE -ne 0) { throw 'Behavior test compilation failed' }
& build/windows/BehaviorTests.exe
if ($LASTEXITCODE -ne 0) { throw 'Behavior tests failed' }
$output = Join-Path $PWD 'build/windows-verification'
$process = Start-Process -FilePath (Join-Path $PWD 'build/windows/DesktopPets.exe') -ArgumentList @('--self-test', ('"' + $output + '"')) -Wait -PassThru
Get-Content (Join-Path $output 'windows-self-test.txt')
if ($process.ExitCode -ne 0) { throw 'Native Windows integration tests failed' }
