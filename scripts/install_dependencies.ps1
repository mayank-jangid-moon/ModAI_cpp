# ModAI Dependency Installation Script for Windows
# Requires PowerShell 5.1 or later

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "ModAI - Dependency Installation Script" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "⚠️  Warning: Not running as administrator. Some installations may require admin rights." -ForegroundColor Yellow
    Write-Host ""
}

# Function to check if command exists
function Test-Command {
    param($Command)
    $null = Get-Command $Command -ErrorAction SilentlyContinue
    return $?
}

# Check for Chocolatey
$hasChoco = Test-Command choco

if (-not $hasChoco) {
    Write-Host "Chocolatey package manager not found." -ForegroundColor Yellow
    Write-Host "Would you like to install Chocolatey? (Y/N)" -ForegroundColor Yellow
    $response = Read-Host
    
    if ($response -eq "Y" -or $response -eq "y") {
        Write-Host "Installing Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
        $hasChoco = $true
    } else {
        Write-Host "⚠️  Please install dependencies manually:" -ForegroundColor Yellow
        Write-Host "   1. CMake: https://cmake.org/download/"
        Write-Host "   2. Qt6: https://www.qt.io/download"
        Write-Host "   3. Visual Studio 2019+ or MinGW"
        exit 1
    }
}

Write-Host "Installing dependencies using Chocolatey..." -ForegroundColor Green
Write-Host ""

# Install CMake
if (-not (Test-Command cmake)) {
    Write-Host "Installing CMake..." -ForegroundColor Cyan
    choco install cmake -y
} else {
    Write-Host "CMake already installed: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
}

# Install Qt6 (via Chocolatey or manual)
Write-Host ""
Write-Host "Qt6 Installation:" -ForegroundColor Cyan
Write-Host "  Option 1: Install via Chocolatey (may take a while)"
Write-Host "  Option 2: Download from https://www.qt.io/download"
Write-Host ""
Write-Host "For now, please install Qt6 manually from https://www.qt.io/download" -ForegroundColor Yellow
Write-Host "  Make sure to install: Qt 6.x, MSVC 2019 64-bit or MinGW" -ForegroundColor Yellow

# Install Visual Studio Build Tools (if not present)
if (-not (Test-Command cl)) {
    Write-Host ""
    Write-Host "Visual Studio Build Tools not found." -ForegroundColor Yellow
    Write-Host "Installing Visual Studio Build Tools..." -ForegroundColor Cyan
    choco install visualstudio2019buildtools -y --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools"
}

# Install Git (for nlohmann/json if needed)
if (-not (Test-Command git)) {
    Write-Host "Installing Git..." -ForegroundColor Cyan
    choco install git -y
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Verifying installations..." -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Verify CMake
Write-Host -NoNewline "CMake: "
if (Test-Command cmake) {
    cmake --version | Select-Object -First 1
} else {
    Write-Host "❌ NOT FOUND" -ForegroundColor Red
}

# Verify Qt6
Write-Host -NoNewline "Qt6: "
$qtPaths = @(
    "$env:ProgramFiles\Qt",
    "${env:ProgramFiles(x86)}\Qt",
    "$env:LOCALAPPDATA\Qt"
)

$qtFound = $false
foreach ($path in $qtPaths) {
    if (Test-Path $path) {
        Write-Host "✅ Found at: $path" -ForegroundColor Green
        $qtFound = $true
        break
    }
}

if (-not $qtFound) {
    Write-Host "⚠️  Not found in standard locations" -ForegroundColor Yellow
    Write-Host "   Please install Qt6 from https://www.qt.io/download" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Installation complete!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Install Qt6 from https://www.qt.io/download if not already installed"
Write-Host "2. Download nlohmann/json header from: https://github.com/nlohmann/json/releases"
Write-Host "3. Set up API keys (see SETUP.md)"
Write-Host "4. Run: .\scripts\build.ps1"
Write-Host "5. Run: .\scripts\run.ps1"
Write-Host ""

