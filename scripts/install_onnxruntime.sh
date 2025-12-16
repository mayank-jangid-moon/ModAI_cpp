#!/bin/bash

# Install ONNX Runtime for ModAI Local AI Detection
# Supports Linux (x64) and macOS (ARM64/x64)

set -e

echo "================================"
echo "ONNX Runtime Installation Script"
echo "================================"
echo ""

VERSION="1.16.3"
INSTALL_DIR="/usr/local"

# Detect OS and architecture
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    ARCH="x64"
    LIB_EXT="so"
    PACKAGE="onnxruntime-linux-x64-${VERSION}.tgz"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="osx"
    if [[ $(uname -m) == "arm64" ]]; then
        ARCH="arm64"
    else
        ARCH="x64"
    fi
    LIB_EXT="dylib"
    PACKAGE="onnxruntime-osx-${ARCH}-${VERSION}.tgz"
else
    echo "❌ Unsupported OS: $OSTYPE"
    exit 1
fi

echo "Detected: $OS ($ARCH)"
echo "Package: $PACKAGE"
echo ""

# Download ONNX Runtime
DOWNLOAD_URL="https://github.com/microsoft/onnxruntime/releases/download/v${VERSION}/${PACKAGE}"

echo "📥 Downloading ONNX Runtime v${VERSION}..."
if command -v wget &> /dev/null; then
    wget -q --show-progress "$DOWNLOAD_URL"
elif command -v curl &> /dev/null; then
    curl -L -O "$DOWNLOAD_URL"
else
    echo "❌ Neither wget nor curl found. Please install one."
    exit 1
fi

# Extract
echo "📦 Extracting..."
tar -xzf "$PACKAGE"

# Get the extracted directory name
EXTRACTED_DIR="${PACKAGE%.tgz}"

# Install
echo "🔧 Installing to $INSTALL_DIR..."

if [ "$EUID" -ne 0 ]; then
    echo "⚠️  Need sudo permissions to install to $INSTALL_DIR"
    sudo cp -r "${EXTRACTED_DIR}/include/"* "$INSTALL_DIR/include/"
    sudo cp "${EXTRACTED_DIR}/lib/"* "$INSTALL_DIR/lib/"
else
    cp -r "${EXTRACTED_DIR}/include/"* "$INSTALL_DIR/include/"
    cp "${EXTRACTED_DIR}/lib/"* "$INSTALL_DIR/lib/"
fi

# Update library cache (Linux only)
if [[ "$OS" == "linux" ]]; then
    echo "🔄 Updating library cache..."
    if [ "$EUID" -ne 0 ]; then
        sudo ldconfig
    else
        ldconfig
    fi
fi

# Cleanup
echo "🧹 Cleaning up..."
rm -rf "$PACKAGE" "$EXTRACTED_DIR"

# Verify installation
echo ""
echo "✅ ONNX Runtime installed successfully!"
echo ""
echo "Installed files:"
echo "  Headers: $INSTALL_DIR/include/onnxruntime/"
echo "  Library: $INSTALL_DIR/lib/libonnxruntime.$LIB_EXT"
echo ""

# Check if library is found
if [[ "$OS" == "linux" ]]; then
    if ldconfig -p | grep -q libonnxruntime; then
        echo "✓ Library is in the system path"
    else
        echo "⚠️  Library might not be in the system path"
        echo "   Try: export LD_LIBRARY_PATH=$INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
    fi
elif [[ "$OS" == "osx" ]]; then
    if [ -f "$INSTALL_DIR/lib/libonnxruntime.$LIB_EXT" ]; then
        echo "✓ Library installed"
    else
        echo "⚠️  Library not found at expected location"
    fi
fi

echo ""
echo "Next steps:"
echo "1. Rebuild ModAI: cd build && cmake .. && cmake --build ."
echo "2. Export AI model: python3 scripts/export_model_to_onnx.py"
echo "3. See docs/LOCAL_AI_SETUP.md for complete setup guide"
