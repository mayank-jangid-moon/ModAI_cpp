# Scripts Directory

This directory contains helper scripts for building and running ModAI.

## Available Scripts

### Installation Scripts

- **`install_dependencies.sh`** (macOS/Linux)
  - Automatically installs all required dependencies
  - Supports Homebrew (macOS) and pacman/apt-get/yum (Linux)
  - Verifies installations after completion

- **`install_dependencies.ps1`** (Windows)
  - Installs dependencies using Chocolatey
  - Guides through Qt6 manual installation
  - Verifies installations

### Build Scripts

- **`build.sh`** (macOS/Linux)
  - Configures CMake
  - Builds the project in Release mode
  - Handles Qt6 path detection automatically

- **`build.ps1`** (Windows)
  - Configures CMake for Windows
  - Builds the project in Release mode
  - Attempts to find Qt6 installation

### Run Scripts

- **`run.sh`** (macOS/Linux)
  - Runs the compiled application
  - Checks for API keys
  - Provides error messages if executable not found

- **`run.ps1`** (Windows)
  - Runs the compiled application
  - Checks for API keys
  - Provides error messages if executable not found

## Usage

### macOS / Linux

```bash
# Make scripts executable (first time only)
chmod +x scripts/*.sh

# Install dependencies
./scripts/install_dependencies.sh

# Build project
./scripts/build.sh

# Run application
./scripts/run.sh
```

### Windows

```powershell
# Install dependencies
.\scripts\install_dependencies.ps1

# Build project
.\scripts\build.ps1

# Run application
.\scripts\run.ps1
```

## Requirements

### macOS / Linux Scripts
- Bash shell
- sudo access (for package installation)
- Internet connection

### Windows Scripts
- PowerShell 5.1 or later
- Administrator access (for some installations)
- Internet connection

## Troubleshooting

### Scripts won't run

**macOS/Linux**: Make sure scripts are executable:
```bash
chmod +x scripts/*.sh
```

**Windows**: Make sure execution policy allows scripts:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Dependencies not found

The scripts attempt to find dependencies automatically, but if they fail:

1. Install dependencies manually (see SETUP.md)
2. Set environment variables (e.g., `Qt6_DIR`)
3. Run scripts again

### Build fails

1. Check that all dependencies are installed
2. Verify CMake version: `cmake --version`
3. Check Qt6 installation path
4. Review build output for specific errors

## Notes

- Scripts are designed to be idempotent (safe to run multiple times)
- Scripts will skip already-installed dependencies
- All scripts provide verbose output for debugging
- Scripts check for prerequisites before running

