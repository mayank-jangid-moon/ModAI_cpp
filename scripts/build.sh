#!/bin/bash

# ModAI Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo "ModAI - Build Script"
echo "========================================="
echo ""

cd "$PROJECT_ROOT"

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure CMake
echo "Configuring CMake..."
echo ""

# Try to find Qt6
if [ -z "$Qt6_DIR" ]; then
    # Common Qt6 locations
    if [ -d "/opt/homebrew/lib/cmake/Qt6" ]; then
        export Qt6_DIR="/opt/homebrew/lib/cmake/Qt6"
        echo "Found Qt6 at: $Qt6_DIR"
    elif [ -d "/usr/local/lib/cmake/Qt6" ]; then
        export Qt6_DIR="/usr/local/lib/cmake/Qt6"
        echo "Found Qt6 at: $Qt6_DIR"
    elif [ -d "/usr/lib/cmake/Qt6" ]; then
        export Qt6_DIR="/usr/lib/cmake/Qt6"
        echo "Found Qt6 at: $Qt6_DIR"
    fi
fi

# Run CMake
if [ -n "$Qt6_DIR" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="$Qt6_DIR"
else
    echo "Qt6_DIR not set, CMake will try to find it automatically..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake configuration failed!"
    echo ""
    echo "Troubleshooting:"
    echo "1. Make sure Qt6 is installed: ./scripts/install_dependencies.sh"
    echo "2. Set Qt6_DIR manually: export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6"
    echo "3. Check CMakeLists.txt for required packages"
    exit 1
fi

echo ""
echo "Building project..."
echo ""

# Build
cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed!"
    exit 1
fi

echo ""
echo "========================================="
echo "Build successful!"
echo "========================================="
echo ""
echo "Executable location: $BUILD_DIR/ModAI"
echo ""
echo "To run the application:"
echo "  ./scripts/run.sh"
echo ""

