#!/usr/bin/env pwsh
# Backend Diagnostics Script for RealSense Viewer

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "  RealSense Viewer - Backend Diagnostics" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""

# Check 1: FastAPI executable
Write-Host "[1/5] Checking FastAPI executable..." -ForegroundColor Yellow
$exePath = "C:\work\librealsense\build\rest-api-dist\realsense_api\realsense_api.exe"
if (Test-Path $exePath) {
    Write-Host "  [OK] Found at: $exePath" -ForegroundColor Green
} else {
    Write-Host "  [X] Not found" -ForegroundColor Red
    Write-Host "  --> Run: .\build-all.ps1" -ForegroundColor Yellow
}
Write-Host ""

# Check 2: Port 8000
Write-Host "[2/5] Checking port 8000..." -ForegroundColor Yellow
try {
    $portCheck = Test-NetConnection -ComputerName 127.0.0.1 -Port 8000 -InformationLevel Quiet -WarningAction SilentlyContinue
    if ($portCheck) {
        Write-Host "  [!] Port 8000 is in use" -ForegroundColor Yellow
    } else {
        Write-Host "  [OK] Port 8000 available" -ForegroundColor Green
    }
} catch {
    Write-Host "  [!] Cannot check port" -ForegroundColor Yellow
}
Write-Host ""

# Check 3: RealSense SDK
Write-Host "[3/5] Checking RealSense SDK..." -ForegroundColor Yellow
$sdkFound = $false
$sdkPaths = @(
    "C:\Program Files (x86)\Intel RealSense SDK 2.0",
    "$env:ProgramFiles\Intel RealSense SDK 2.0"
)
foreach ($path in $sdkPaths) {
    if (Test-Path $path) {
        Write-Host "  [OK] Found SDK at: $path" -ForegroundColor Green
        $sdkFound = $true
        break
    }
}
if (-not $sdkFound) {
    Write-Host "  [X] SDK not found" -ForegroundColor Red
}
Write-Host ""

# Check 4: Test executable
Write-Host "[4/5] Testing executable manually..." -ForegroundColor Yellow
if (Test-Path $exePath) {
    Write-Host "  --> Starting (5sec test)..." -ForegroundColor Gray
    $testProc = Start-Process -FilePath $exePath -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 5
    
    if ($testProc.HasExited) {
        Write-Host "  [X] Process exited (code: $($testProc.ExitCode))" -ForegroundColor Red
    } else {
        Write-Host "  [OK] Process running (PID: $($testProc.Id))" -ForegroundColor Green
        try {
            $response = Invoke-WebRequest -Uri "http://127.0.0.1:8000/api/v1/health" -TimeoutSec 2 -UseBasicParsing
            Write-Host "  [OK] Health check passed" -ForegroundColor Green
        } catch {
            Write-Host "  [!] Health check failed" -ForegroundColor Yellow
        }
        Stop-Process -Id $testProc.Id -Force
    }
} else {
    Write-Host "  [!] Skipped - exe not found" -ForegroundColor Yellow
}
Write-Host ""

# Check 5: Installers
Write-Host "[5/5] Checking installers..." -ForegroundColor Yellow
$found = $false
$paths = @(
    "C:\work\librealsense\build\tauri-target\release\bundle\msi\",
    "C:\work\librealsense\build\tauri-target\release\bundle\nsis\"
)
foreach ($p in $paths) {
    if (Test-Path $p) {
        $installers = Get-ChildItem -Path $p -Filter "*.msi","*.exe" -ErrorAction SilentlyContinue
        if ($installers) {
            Write-Host "  [OK] Found: $p" -ForegroundColor Green
            $found = $true
        }
    }
}
if (-not $found) {
    Write-Host "  [!] No installers found" -ForegroundColor Yellow
}
Write-Host ""

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "  1. If exe missing: .\build-all.ps1" -ForegroundColor Gray
Write-Host "  2. If SDK missing: Install SDK from releases" -ForegroundColor Gray
Write-Host "  3. Try manual run: $exePath" -ForegroundColor Gray
Write-Host ""
