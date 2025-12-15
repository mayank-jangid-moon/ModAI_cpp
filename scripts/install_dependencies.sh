#!/bin/bash

# ModAI Dependency Installation Script
# Supports macOS (Homebrew) and Linux (apt-get/yum)

set -e

echo "========================================="
echo "ModAI - Dependency Installation Script"
echo "========================================="
echo ""

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
    echo "Detected: macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
    echo "Detected: Linux"
else
    echo "Error: Unsupported OS. This script supports macOS and Linux."
    exit 1
fi

# Check for package manager
if [ "$OS" == "macOS" ]; then
    if ! command -v brew &> /dev/null; then
        echo "Error: Homebrew not found. Please install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    PKG_MGR="brew"
elif [ "$OS" == "Linux" ]; then
    if command -v apt-get &> /dev/null; then
        PKG_MGR="apt"
    elif command -v yum &> /dev/null; then
        PKG_MGR="yum"
    else
        echo "Error: No supported package manager found (apt-get or yum)"
        exit 1
    fi
fi

echo "Using package manager: $PKG_MGR"
echo ""

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install on macOS
install_macos() {
    echo "Installing dependencies on macOS..."
    echo ""
    
    # Update Homebrew
    echo "Updating Homebrew..."
    brew update
    
    # Install CMake
    if ! command_exists cmake; then
        echo "Installing CMake..."
        brew install cmake
    else
        echo "CMake already installed: $(cmake --version | head -n1)"
    fi
    
    # Install Qt6
    if ! command_exists qmake6 || [ -z "$(qmake6 -v 2>/dev/null)" ]; then
        echo "Installing Qt6..."
        brew install qt6
    else
        echo "Qt6 already installed"
    fi
    
    # Install nlohmann/json (header-only, via Homebrew)
    if [ ! -d "/opt/homebrew/include/nlohmann" ] && [ ! -d "/usr/local/include/nlohmann" ]; then
        echo "Installing nlohmann/json..."
        brew install nlohmann-json
    else
        echo "nlohmann/json already installed"
    fi
    
    # Install OpenSSL (usually comes with macOS, but ensure it's available)
    if ! command_exists openssl; then
        echo "Installing OpenSSL..."
        brew install openssl
    else
        echo "OpenSSL already installed: $(openssl version)"
    fi
    
    # Install build tools
    if ! command_exists g++; then
        echo "Installing Xcode Command Line Tools..."
        xcode-select --install || true
    fi
    
    echo ""
    echo "macOS dependencies installed!"
}

# Function to install on Linux (apt)
install_linux_apt() {
    echo "Installing dependencies on Linux (apt)..."
    echo ""
    
    # Update package list
    echo "Updating package list..."
    sudo apt-get update
    
    # Install build essentials
    echo "Installing build essentials..."
    sudo apt-get install -y build-essential
    
    # Install CMake
    if ! command_exists cmake; then
        echo "Installing CMake..."
        sudo apt-get install -y cmake
    else
        echo "CMake already installed: $(cmake --version | head -n1)"
    fi
    
    # Install Qt6
    echo "Installing Qt6..."
    sudo apt-get install -y qt6-base-dev qt6-charts-dev qt6-network-dev
    
    # Install nlohmann/json
    if ! dpkg -l | grep -q nlohmann-json3-dev; then
        echo "Installing nlohmann/json..."
        sudo apt-get install -y nlohmann-json3-dev
    else
        echo "nlohmann/json already installed"
    fi
    
    # Install OpenSSL
    if ! command_exists openssl; then
        echo "Installing OpenSSL..."
        sudo apt-get install -y libssl-dev
    else
        echo "OpenSSL already installed: $(openssl version)"
    fi
    
    echo ""
    echo "Linux (apt) dependencies installed!"
}

# Function to install on Linux (yum)
install_linux_yum() {
    echo "Installing dependencies on Linux (yum)..."
    echo ""
    
    # Install build tools
    echo "Installing build essentials..."
    sudo yum groupinstall -y "Development Tools"
    
    # Install CMake
    if ! command_exists cmake; then
        echo "Installing CMake..."
        sudo yum install -y cmake
    else
        echo "CMake already installed: $(cmake --version | head -n1)"
    fi
    
    # Install Qt6 (may need EPEL or other repos)
    echo "Installing Qt6..."
    sudo yum install -y qt6-qtbase-devel qt6-qtcharts-devel qt6-qtnetwork-devel || {
        echo "Warning: Qt6 packages may not be available in default repos."
        echo "You may need to add EPEL or install Qt6 manually."
    }
    
    # Install OpenSSL
    if ! command_exists openssl; then
        echo "Installing OpenSSL..."
        sudo yum install -y openssl-devel
    else
        echo "OpenSSL already installed: $(openssl version)"
    fi
    
    echo ""
    echo "Note: nlohmann/json may need to be installed manually on yum-based systems"
    echo "Download from: https://github.com/nlohmann/json/releases"
    echo ""
    echo "Linux (yum) dependencies installed!"
}

# Main installation
echo "Starting installation..."
echo ""

if [ "$OS" == "macOS" ]; then
    install_macos
elif [ "$OS" == "Linux" ] && [ "$PKG_MGR" == "apt" ]; then
    install_linux_apt
elif [ "$OS" == "Linux" ] && [ "$PKG_MGR" == "yum" ]; then
    install_linux_yum
fi

echo ""
echo "========================================="
echo "Verifying installations..."
echo "========================================="

# Verify installations
echo -n "CMake: "
if command_exists cmake; then
    cmake --version | head -n1
else
    echo "NOT FOUND"
fi

echo -n "Qt6: "
if command_exists qmake6; then
    qmake6 -v 2>/dev/null | head -n1 || echo "Installed"
else
    echo "qmake6 not in PATH (Qt6 may still be installed)"
fi

echo -n "OpenSSL: "
if command_exists openssl; then
    openssl version
else
    echo "NOT FOUND"
fi

echo -n "nlohmann/json: "
if [ -d "/opt/homebrew/include/nlohmann" ] || [ -d "/usr/local/include/nlohmann" ] || [ -d "/usr/include/nlohmann" ]; then
    echo "Found"
else
    echo "Not found in standard locations"
fi

echo ""
echo "========================================="
echo "Installation complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo "1. Set up API keys (see SETUP.md)"
echo "2. Run: ./scripts/build.sh"
echo "3. Run: ./scripts/run.sh"
echo ""

