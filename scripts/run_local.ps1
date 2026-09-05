# Launch VoIP Translation App local processes on Windows

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Launching Local VoIP Translation Engine" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Start C++ Transmission App
Write-Host "--> Starting C++ Transmission App on UDP :5004..." -ForegroundColor Yellow
$transExe = "$RootDir\transmission\build\Release\transmission_app.exe"
if (!(Test-Path $transExe)) {
    $transExe = "$RootDir\transmission\build\Debug\transmission_app.exe"
}

if (Test-Path $transExe) {
    $transProcess = Start-Process -FilePath $transExe -ArgumentList "--local-port 5004 --remote-port 5006" -PassThru
} else {
    Write-Warning "transmission_app.exe not found. Run .\scripts\build_all.ps1 first."
}

Start-Sleep -Seconds 1

# 2. Start Python Processing App
Write-Host "--> Starting Python Sarvam Edge Processing App..." -ForegroundColor Yellow
$pyExe = "$RootDir\processing\venv\Scripts\python.exe"
if (!(Test-Path $pyExe)) { $pyExe = "python" }

$procProcess = Start-Process -FilePath $pyExe -ArgumentList "$RootDir\processing\src\main.py --source-lang hi-IN --target-lang en-IN --chunk-ms 300" -PassThru

Start-Sleep -Seconds 1

# 3. Start Flutter Desktop App
Write-Host "--> Starting Flutter Desktop Application..." -ForegroundColor Yellow
Set-Location "$RootDir\frontend"
flutter run -d windows

# Cleanup when Flutter app exits
if ($transProcess -and !$transProcess.HasExited) { Stop-Process -Id $transProcess.Id -Force }
if ($procProcess -and !$procProcess.HasExited) { Stop-Process -Id $procProcess.Id -Force }
