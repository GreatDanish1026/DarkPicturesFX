# Builds DarkPicturesFX.addon64 with MSVC (Visual Studio 2022+ with C++ tools). MSVC is
# required: ReShade is built with it, and MinGW returns API structs differently.
# Usage: .\build.ps1 [-Install] [-Package] [-GameBin <path>]
#   -Install  copies the add-on next to the game's shipping exe in -GameBin
#   -Package  makes bin\release\DarkPicturesFX-<version>.zip: the add-on, README, LICENSE and
#             THIRD_PARTY_NOTICES.txt
param(
    [switch]$Install,
    [switch]$Package,
    [string]$GameBin = "E:\SteamLibrary\steamapps\common\The Dark Pictures Anthology - House of Ashes\SMG025\Binaries\Win64"
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$name = "DarkPicturesFX"
$addon = "$root\bin\$name.addon64"
# Games it installs into: the shipping executables of The Dark Pictures Anthology (and The
# Quarry, the same engine).
$exes = "HouseOfAshes-Win64-Shipping.exe", "TheDevilInMe-Win64-Shipping.exe", "LittleHope-Win64-Shipping.exe",
        "ManOfMedan-Win64-Shipping.exe", "TheQuarry-Win64-Shipping.exe"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with the C++ x64 tools not found." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

$obj = "$root\bin\obj"
New-Item -ItemType Directory -Force $obj | Out-Null

# ReShade add-on headers in third_party\reshade match ReShade 6.8.0.
$cmd = @(
    "call `"$vcvars`" >nul",
    "rc /nologo /i `"$root\src`" /fo `"$obj\version.res`" `"$root\src\version.rc`"",
    ("cl /nologo /std:c++17 /O2 /EHsc /MT /W3 /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX " +
     "/I `"$root\third_party\imgui`" /I `"$root\third_party\reshade\include`" " +
     "/Fo`"$obj\\`" `"$root\src\fx.cpp`" `"$obj\version.res`" " +
     "/link /DLL /OUT:`"$addon`" /PDB:`"$obj\$name.pdb`" /DEBUG")
) -join " && "
cmd /c $cmd
if ($LASTEXITCODE) { throw "add-on build failed" }
$version = (Get-Item $addon).VersionInfo.FileVersion  # QDLSS_VERSION from src\version.h
Write-Host "Built $addon ($name $version)"

if ($Install) {
    $exe = $exes | Where-Object { Test-Path "$GameBin\$_" } | Select-Object -First 1
    if (-not $exe) { throw "None of $($exes -join ', ') found in $GameBin (pass -GameBin)" }
    if (Get-Process ([IO.Path]::GetFileNameWithoutExtension($exe)) -ErrorAction SilentlyContinue) { throw "Close the game first." }
    Copy-Item $addon $GameBin -Force
    Write-Host "Installed $name to $GameBin"
}

if ($Package) {
    $stage = "$root\bin\release\$name-$version"
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item $addon, "$root\README.md", "$root\LICENSE", "$root\THIRD_PARTY_NOTICES.txt" $stage
    $zip = "$root\bin\release\$name-$version.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path "$stage\*" -DestinationPath $zip
    Write-Host "Packaged $zip"
    Get-ChildItem $stage -File | ForEach-Object { "{0,-32} {1,-10} {2,10:N0}" -f $_.Name, $_.VersionInfo.FileVersion, $_.Length }
}
