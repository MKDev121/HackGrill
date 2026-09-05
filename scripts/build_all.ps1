# Build all VoIP Translation App components on Windows

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building VoIP Translation App (All Components)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Build C++ Transmission App
Write-Host "--> [1/3] Building C++ Transmission App..." -ForegroundColor Yellow
Set-Location "$RootDir\transmission"
if (!(Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }
Set-Location "$RootDir\transmission\build"
cmake ..
cmake --build . --config Release

# 2. Setup Python Processing Environment
Write-Host "--> [2/3] Setting up Python Processing Environment..." -ForegroundColor Yellow
Set-Location "$RootDir\processing"
if (!(Test-Path "venv")) { python -m venv venv }
& "$RootDir\processing\venv\Scripts\Activate.ps1"
pip install --upgrade pip
pip install -r requirements.txt

# 3. Build Flutter Desktop Frontend
Write-Host "--> [3/3] Getting Flutter Dependencies..." -ForegroundColor Yellow
Set-Location "$RootDir\frontend"
flutter pub get

Write-Host "==========================================================" -ForegroundColor Green
Write-Host " All components built successfully!" -ForegroundColor Green
Write-Host " Run '.\scripts\run_local.ps1' to launch." -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
