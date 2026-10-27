param([switch]$Test)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
Push-Location $project
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vs = if (Test-Path $vswhere) { & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
    if ($vs) {
        $dev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
        $commands = @('@echo off', "call `"$dev`" -arch=x64 >nul", 'if not "%errorlevel%"=="0" exit /b 1', 'cl /nologo /std:c++14 /EHsc /utf-8 /W4 /Zi /Febuild\logflow.exe /Fobuild\main.obj /Fdbuild\logflow.pdb main.cpp', 'if not "%errorlevel%"=="0" exit /b 1')
        if ($Test) {
            foreach ($file in Get-ChildItem tests/*Tests.cpp,tests/IntegrationTest.cpp) {
                $name = $file.BaseName
                $commands += "cl /nologo /std:c++14 /EHsc /utf-8 /W4 /Fe`"build\$name.exe`" /Fo`"build\$name.obj`" `"tests\$($file.Name)`""
                $commands += 'if not "%errorlevel%"=="0" exit /b 1'
                $commands += "build\$name.exe"
                $commands += 'if not "%errorlevel%"=="0" exit /b 1'
            }
        }
        $commands | Set-Content -Encoding ascii build/compile.cmd
        & cmd.exe /d /c build\compile.cmd
        if ($LASTEXITCODE -ne 0) { throw 'Derleme veya test basarisiz.' }
    } else {
        & g++ -std=c++11 -Wall -Wextra -pedantic -g -o build/logflow.exe main.cpp
        if ($LASTEXITCODE -ne 0) { throw 'C++ derlemesi basarisiz. MSVC veya g++ kurulumunu kontrol edin.' }
        if ($Test) {
            foreach ($file in Get-ChildItem tests/*Tests.cpp,tests/IntegrationTest.cpp) {
                $exe = "build/$($file.BaseName).exe"
                & g++ -std=c++11 -Wall -Wextra -pedantic -o $exe $file.FullName
                if ($LASTEXITCODE -ne 0) { throw "Derlenemedi: $($file.Name)" }
                & "./$exe"
                if ($LASTEXITCODE -ne 0) { throw "Test basarisiz: $($file.Name)" }
            }
        }
    }
    Copy-Item -LiteralPath build/logflow.exe -Destination logflow.exe -Force
} finally { Pop-Location }
