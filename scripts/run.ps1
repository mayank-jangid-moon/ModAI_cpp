# ModAI Run Script for Windows

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build"
$Executable = Join-Path $BuildDir "ModAI.exe"

# Check if executable exists
if (-not (Test-Path $Executable)) {
    Write-Host "ERROR: Executable not found: $Executable" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please build the project first:" -ForegroundColor Yellow
    Write-Host "  .\scripts\build.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "ModAI - Trust & Safety Dashboard" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Starting application..." -ForegroundColor Green
Write-Host ""

# Check for API keys
if (-not $env:MODAI_HUGGINGFACE_TOKEN -and -not $env:MODAI_HIVE_API_KEY) {
    Write-Host "Warning: API keys not set in environment variables" -ForegroundColor Yellow
    Write-Host "   The app will look for keys in config file" -ForegroundColor Yellow
    Write-Host "   See SETUP.md for configuration instructions" -ForegroundColor Yellow
    Write-Host ""
}

# Run the application
Set-Location $ProjectRoot
& $Executable

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Application exited with error code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "1. Check that all dependencies are installed"
    Write-Host "2. Verify API keys are configured (see SETUP.md)"
    Write-Host "3. Check logs in: data\logs\system.log"
    exit $LASTEXITCODE
}

