param(
    [switch]$build,
    [switch]$run,
    [switch]$clean,
    [switch]$release,
    [switch]$tests
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"
$BuildType = if ($release) { "Release" } else { "Debug" }

function Write-ColorOutput($ForegroundColor, $message) {
    Write-Host $message -ForegroundColor $ForegroundColor
}

# Clean build folder
if ($clean) {
    Write-ColorOutput Yellow "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-ColorOutput Green "Build directory cleaned"
    } else {
        Write-ColorOutput Cyan "Build directory doesn't exist"
    }
    if (-not $build -and -not $run) {
        exit 0
    }
}

# Build the project
if ($build -or $run) {
    Write-ColorOutput Cyan "Building Jar Renderer in $BuildType mode..."

    # Create build directory if it doesn't exist
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    # Configure with CMake
    Write-ColorOutput Yellow "Configuring CMake..."

    # Check for vcpkg toolchain
    $vcpkgToolchain = ""

    # Check if vcpkg is available and find toolchain
    $vcpkgExe = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($vcpkgExe) {
        $vcpkgRoot = Split-Path -Parent (Split-Path -Parent $vcpkgExe.Source)
        $toolchainPath = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchainPath) {
            $vcpkgToolchain = "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainPath`""
            Write-ColorOutput Green "Using vcpkg toolchain: $toolchainPath"
        }
    }

    # Try environment variable
    if (-not $vcpkgToolchain -and $env:VCPKG_ROOT) {
        $toolchainPath = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchainPath) {
            $vcpkgToolchain = "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainPath`""
            Write-ColorOutput Green "Using vcpkg toolchain from VCPKG_ROOT: $toolchainPath"
        }
    }

    # Check common locations
    if (-not $vcpkgToolchain) {
        $commonPaths = @(
            "C:\vcpkg\scripts\buildsystems\vcpkg.cmake",
            "C:\tools\vcpkg\scripts\buildsystems\vcpkg.cmake",
            "$env:LOCALAPPDATA\vcpkg\scripts\buildsystems\vcpkg.cmake"
        )

        foreach ($path in $commonPaths) {
            if (Test-Path $path) {
                $vcpkgToolchain = "-DCMAKE_TOOLCHAIN_FILE=`"$path`""
                Write-ColorOutput Green "Using vcpkg toolchain: $path"
                break
            }
        }
    }

    if (-not $vcpkgToolchain) {
        Write-ColorOutput Yellow "vcpkg toolchain not found - some dependencies may not be available"
    }

    Push-Location $BuildDir
try {
    # Build the cmake command with vcpkg toolchain if available
    # Using Ninja generator with CL compiler, and export compile_commands.json
    $testFlag = if ($tests) { "-DBUILD_TESTS=ON" } else { "" }
    $cmakeCmd = "cmake .. -G `"Ninja`" -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $testFlag $vcpkgToolchain"
    Invoke-Expression $cmakeCmd
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build with CMake
    Write-ColorOutput Yellow "Building project..."
    cmake --build . --config $BuildType --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-ColorOutput Green "Build completed successfully"

    # Run tests if requested
    if ($tests) {
        Write-ColorOutput Cyan "Running unit tests..."
        Write-ColorOutput Yellow "================================"
        ctest --output-on-failure --parallel
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput Red "Some tests failed!"
        } else {
            Write-ColorOutput Green "All tests passed!"
        }
        Write-ColorOutput Yellow "================================"
    }
} finally {
    Pop-Location
}
}

# Run the executable
if ($run) {
    $ExePath = Join-Path $BuildDir "Jar.exe"

    if (Test-Path $ExePath) {
        Write-ColorOutput Cyan "Running Jar Renderer..."
        Write-ColorOutput Yellow "================================"
        & $ExePath
        Write-ColorOutput Yellow "================================"
        Write-ColorOutput Green "Application exited"
    } else {
        Write-ColorOutput Red "Executable not found at: $ExePath"
        exit 1
    }
}

if (-not $build -and -not $run -and -not $clean -and -not $tests) {
    Write-ColorOutput Yellow "Usage: .\build.ps1 [-build] [-run] [-clean] [-release] [-tests]"
    Write-ColorOutput Cyan "  -build    : Build the project (debug mode by default)"
    Write-ColorOutput Cyan "  -run      : Build and run the project"
    Write-ColorOutput Cyan "  -clean    : Clean the build directory"
    Write-ColorOutput Cyan "  -release  : Build in release mode (use with -build or -run)"
    Write-ColorOutput Cyan "  -tests    : Build and run unit tests"
}

if ($tests) {
    $build = $true
}
