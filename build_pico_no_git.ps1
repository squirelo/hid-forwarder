# PowerShell script to build Pico firmware using Docker (No Git Submodules)
# Make sure Docker Desktop is running before executing this script

Write-Host "Building Pico firmware using Docker (No Git Submodules)..." -ForegroundColor Green

# Check if Docker is running
try {
    docker ps > $null 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Docker is not running. Please start Docker Desktop and try again." -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "Docker is not running. Please start Docker Desktop and try again." -ForegroundColor Red
    exit 1
}

# Navigate to the receiver-pico directory
Set-Location "receiver-pico"

# Clean build directory if it exists to avoid CMake cache conflicts
if (Test-Path "build") {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

# Create build directory
New-Item -ItemType Directory -Name "build"

# Run the build using Docker without git submodules
Write-Host "Starting Docker build..." -ForegroundColor Yellow
docker run --rm -v "${PWD}:/workspace" -w /workspace ubuntu:22.04 bash -c "apt-get update && apt-get install -y --no-install-recommends gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib cmake git make python3 ca-certificates g++ && git config --global http.sslVerify false && mkdir -p build-pico_w && cd build-pico_w && PICO_BOARD=pico_w cmake .. && make -j4 && cd .. && mkdir -p artifacts && cp build-pico_w/receiver.uf2 artifacts/receiver_pico_w.uf2 && echo 'Build completed successfully!' && echo 'Firmware files:' && ls -la artifacts/"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Firmware files are in the receiver-pico/artifacts directory:" -ForegroundColor Green
    if (Test-Path "artifacts") {
        Get-ChildItem "artifacts" | ForEach-Object {
            Write-Host "  - $($_.Name)" -ForegroundColor Cyan
        }
    }
} else {
    Write-Host "Build failed. Check the error messages above." -ForegroundColor Red
}

# Go back to the original directory
Set-Location ".." 