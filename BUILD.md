# Build Instructions

## Quick Start

```bash
mkdir build && cd build
cmake ..
cmake --build .
./ModAI  # or ModAI.exe on Windows
```

## Detailed Build Steps

### 1. Install Dependencies

#### macOS
```bash
brew install qt6 cmake
# nlohmann/json is header-only, download from GitHub or use:
brew install nlohmann-json
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install qt6-base-dev qt6-charts-dev qt6-network-dev
sudo apt-get install libssl-dev
# nlohmann/json
sudo apt-get install nlohmann-json3-dev
```

#### Windows
1. Install Qt 6 from https://www.qt.io/download
2. Install CMake from https://cmake.org/download/
3. Install Visual Studio 2019+ or MinGW
4. Download nlohmann/json (header-only) from https://github.com/nlohmann/json

### 2. Configure CMake

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```

If Qt6 is not found automatically:
```bash
cmake .. -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6
```

### 3. Build

```bash
cmake --build . --config Release
```

Or use your IDE's build system.

### 4. Run

```bash
./ModAI  # Linux/macOS
ModAI.exe  # Windows
```

## Troubleshooting

### Qt6 Not Found
```bash
# Find Qt6 installation
find /usr -name "Qt6Config.cmake" 2>/dev/null
# or
brew --prefix qt6

# Set in CMake
cmake .. -DQt6_DIR=/usr/local/lib/cmake/Qt6
```

### nlohmann/json Not Found
Download the single header file:
```bash
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
mkdir -p include/nlohmann
mv json.hpp include/nlohmann/json.hpp
```

Then update CMakeLists.txt to use the local include.

### Compilation Errors

**Missing includes**: Ensure all header files are in the `include/` directory with proper structure.

**Link errors**: Verify Qt6 libraries are properly linked:
```bash
ldd ./ModAI | grep Qt
```

**C++17 features**: Ensure your compiler supports C++17:
```bash
g++ --version  # Should be 7.0+
clang++ --version  # Should be 5.0+
```

## Development Build

For development with debug symbols:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Testing

Tests are optional. To build with tests:
```bash
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest
```

