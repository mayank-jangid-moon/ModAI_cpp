# ModAI Build Script for Windows

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "ModAI - Build Script" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Create build directory
if (-not (Test-Path $BuildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Set-Location $BuildDir

# Try to find Qt6
$Qt6Dir = $env:Qt6_DIR
if (-not $Qt6Dir) {
    $commonPaths = @(
        "$env:ProgramFiles\Qt\6.*\msvc*\lib\cmake\Qt6",
        "${env:ProgramFiles(x86)}\Qt\6.*\msvc*\lib\cmake\Qt6",
        "$env:LOCALAPPDATA\Qt\6.*\msvc*\lib\cmake\Qt6"
    )
    
    foreach ($pattern in $commonPaths) {
        $matches = Get-ChildItem -Path (Split-Path $pattern -Parent) -Filter "Qt6Config.cmake" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($matches) {
            $Qt6Dir = Split-Path (Split-Path $matches.FullName -Parent) -Parent
            Write-Host "Found Qt6 at: $Qt6Dir" -ForegroundColor Green
            break
        }
    }
}

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Cyan
Write-Host ""

if ($Qt6Dir) {
    cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="$Qt6Dir"
} else {
    Write-Host "Qt6_DIR not set, CMake will try to find it automatically..." -ForegroundColor Yellow
    cmake .. -DCMAKE_BUILD_TYPE=Release
}

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "1. Make sure Qt6 is installed"
    Write-Host "2. Set Qt6_DIR manually: `$env:Qt6_DIR = 'C:\path\to\qt6\lib\cmake\Qt6'"
    Write-Host "3. Check CMakeLists.txt for required packages"
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Cyan
Write-Host ""

# Build
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Build successful!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Executable location: $BuildDir\ModAI.exe" -ForegroundColor Green
Write-Host ""
Write-Host "To run the application:" -ForegroundColor Yellow
Write-Host "  .\scripts\run.ps1" -ForegroundColor Yellow
Write-Host ""

