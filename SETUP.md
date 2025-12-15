# ModAI Setup Instructions

Complete guide for setting up and running the ModAI Trust & Safety Dashboard.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Detailed Setup](#detailed-setup)
4. [API Key Configuration](#api-key-configuration)
5. [Building the Project](#building-the-project)
6. [Running the Application](#running-the-application)
7. [Troubleshooting](#troubleshooting)
8. [Platform-Specific Notes](#platform-specific-notes)

---

## Prerequisites

### Required Software

- **C++ Compiler**: GCC 7+, Clang 5+, or MSVC 2017+
- **CMake**: Version 3.20 or higher
- **Qt 6**: Widgets, Network, and Charts modules
- **nlohmann/json**: Header-only JSON library
- **OpenSSL**: For TLS/HTTPS support

### API Keys Required

Before running the application, you'll need:

1. **Hugging Face API Token**
   - Sign up at: https://huggingface.co/join
   - Get token from: https://huggingface.co/settings/tokens
   - Used for AI text detection

2. **Hive/TheHive.ai API Key**
   - Sign up at: https://thehive.ai
   - Get API key from your dashboard
   - Used for image and text moderation

3. **Reddit OAuth Credentials**
   - Create app at: https://www.reddit.com/prefs/apps
   - Select "script" as app type
   - Note your Client ID and Secret
   - Used for Reddit API access

---

## Quick Start

### macOS / Linux

```bash
# 1. Install dependencies
./scripts/install_dependencies.sh

# 2. Set API keys (see API Key Configuration section)
export MODAI_HUGGINGFACE_TOKEN="your_token"
export MODAI_HIVE_API_KEY="your_key"
export MODAI_REDDIT_CLIENT_ID="your_id"
export MODAI_REDDIT_CLIENT_SECRET="your_secret"

# 3. Build the project
./scripts/build.sh

# 4. Run the application
./scripts/run.sh
```

### Windows

```powershell
# 1. Install dependencies
.\scripts\install_dependencies.ps1

# 2. Set API keys (see API Key Configuration section)
$env:MODAI_HUGGINGFACE_TOKEN = "your_token"
$env:MODAI_HIVE_API_KEY = "your_key"
$env:MODAI_REDDIT_CLIENT_ID = "your_id"
$env:MODAI_REDDIT_CLIENT_SECRET = "your_secret"

# 3. Build the project
.\scripts\build.ps1

# 4. Run the application
.\scripts\run.ps1
```

---

## Detailed Setup

### Step 1: Install Dependencies

#### macOS (using Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Run the installation script
./scripts/install_dependencies.sh
```

Or manually:
```bash
brew install cmake qt6 nlohmann-json openssl
```

#### Linux (Ubuntu/Debian)

```bash
# Run the installation script
./scripts/install_dependencies.sh
```

Or manually:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install qt6-base-dev qt6-charts-dev qt6-network-dev
sudo apt-get install libssl-dev nlohmann-json3-dev
```

#### Linux (Fedora/RHEL/CentOS)

```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake qt6-qtbase-devel qt6-qtcharts-devel
sudo yum install openssl-devel
# nlohmann/json may need manual installation
```

#### Windows

1. **Install Chocolatey** (if not installed):
   ```powershell
   Set-ExecutionPolicy Bypass -Scope Process -Force
   [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
   iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
   ```

2. **Run installation script**:
   ```powershell
   .\scripts\install_dependencies.ps1
   ```

3. **Install Qt6 manually**:
   - Download from: https://www.qt.io/download
   - Install Qt 6.x with MSVC 2019 64-bit or MinGW
   - Note the installation path (needed for CMake)

4. **Install Visual Studio** (if using MSVC):
   - Download Visual Studio 2019 or later
   - Include "Desktop development with C++" workload

### Step 2: Verify Installations

#### macOS / Linux

```bash
# Check CMake
cmake --version

# Check Qt6
qmake6 -v

# Check OpenSSL
openssl version
```

#### Windows

```powershell
# Check CMake
cmake --version

# Check Qt6 (if in PATH)
qmake -v

# Check Visual Studio
cl
```

---

## API Key Configuration

You can configure API keys in two ways:

### Method 1: Environment Variables (Recommended)

#### macOS / Linux

Add to your `~/.bashrc`, `~/.zshrc`, or session:

```bash
export MODAI_HUGGINGFACE_TOKEN="hf_xxxxxxxxxxxxxxxxxxxxx"
export MODAI_HIVE_API_KEY="your_hive_api_key_here"
export MODAI_REDDIT_CLIENT_ID="your_reddit_client_id"
export MODAI_REDDIT_CLIENT_SECRET="your_reddit_client_secret"
```

Then reload:
```bash
source ~/.bashrc  # or source ~/.zshrc
```

#### Windows

**PowerShell (Session)**:
```powershell
$env:MODAI_HUGGINGFACE_TOKEN = "hf_xxxxxxxxxxxxxxxxxxxxx"
$env:MODAI_HIVE_API_KEY = "your_hive_api_key_here"
$env:MODAI_REDDIT_CLIENT_ID = "your_reddit_client_id"
$env:MODAI_REDDIT_CLIENT_SECRET = "your_reddit_client_secret"
```

**PowerShell (Permanent)**:
```powershell
[System.Environment]::SetEnvironmentVariable("MODAI_HUGGINGFACE_TOKEN", "hf_xxxxxxxxxxxxxxxxxxxxx", "User")
[System.Environment]::SetEnvironmentVariable("MODAI_HIVE_API_KEY", "your_hive_api_key_here", "User")
[System.Environment]::SetEnvironmentVariable("MODAI_REDDIT_CLIENT_ID", "your_reddit_client_id", "User")
[System.Environment]::SetEnvironmentVariable("MODAI_REDDIT_CLIENT_SECRET", "your_reddit_client_secret", "User")
```

**Command Prompt**:
```cmd
setx MODAI_HUGGINGFACE_TOKEN "hf_xxxxxxxxxxxxxxxxxxxxx"
setx MODAI_HIVE_API_KEY "your_hive_api_key_here"
setx MODAI_REDDIT_CLIENT_ID "your_reddit_client_id"
setx MODAI_REDDIT_CLIENT_SECRET "your_reddit_client_secret"
```

### Method 2: Configuration File

The application will create a config file at:

- **macOS**: `~/Library/Preferences/ModAI/config.ini`
- **Linux**: `~/.config/ModAI/config.ini`
- **Windows**: `%APPDATA%\ModAI\config.ini`

Edit the file manually:

```ini
[huggingface_token]
value=your_token_here

[hive_api_key]
value=your_key_here

[reddit_client_id]
value=your_id_here

[reddit_client_secret]
value=your_secret_here
```

**Note**: Currently, keys are stored in plain text. Encryption will be added in a future version.

---

## Building the Project

### Using Scripts (Recommended)

#### macOS / Linux

```bash
./scripts/build.sh
```

#### Windows

```powershell
.\scripts\build.ps1
```

### Manual Build

#### macOS / Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

#### Windows

```powershell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR="C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6"
cmake --build . --config Release
```

**Note**: Replace `C:\Qt\6.x.x\msvc2019_64` with your actual Qt6 installation path.

### Build Options

- **Debug Build**: `cmake .. -DCMAKE_BUILD_TYPE=Debug`
- **Release Build**: `cmake .. -DCMAKE_BUILD_TYPE=Release` (default)
- **Custom Qt6 Path**: `cmake .. -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6`

---

## Running the Application

### Using Scripts (Recommended)

#### macOS / Linux

```bash
./scripts/run.sh
```

#### Windows

```powershell
.\scripts\run.ps1
```

### Manual Run

#### macOS / Linux

```bash
cd build
./ModAI
```

#### Windows

```powershell
cd build
.\ModAI.exe
```

### First Run

1. **Check API Keys**: The app will warn if keys are missing
2. **Configure Rules**: Edit `config/rules.json` if needed
3. **Start Scraping**: Enter a subreddit name and click "Start Scraping"
4. **Monitor Dashboard**: Items will appear as they're processed

---

## Troubleshooting

### Build Issues

#### CMake can't find Qt6

**macOS / Linux**:
```bash
# Find Qt6 installation
find /usr -name "Qt6Config.cmake" 2>/dev/null
# or
brew --prefix qt6

# Set Qt6_DIR
export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6
cmake .. -DQt6_DIR=$Qt6_DIR
```

**Windows**:
```powershell
# Find Qt6 installation
Get-ChildItem -Path "C:\Qt" -Recurse -Filter "Qt6Config.cmake" -ErrorAction SilentlyContinue

# Set Qt6_DIR
$env:Qt6_DIR = "C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6"
cmake .. -DQt6_DIR=$env:Qt6_DIR
```

#### nlohmann/json not found

**Option 1**: Install via package manager
```bash
# macOS
brew install nlohmann-json

# Linux
sudo apt-get install nlohmann-json3-dev
```

**Option 2**: Download header manually
```bash
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
mkdir -p include/nlohmann
mv json.hpp include/nlohmann/json.hpp
```

#### Compilation errors

- **C++17 features**: Ensure your compiler supports C++17
- **Missing includes**: Check that all header files are in `include/` directory
- **Link errors**: Verify Qt6 libraries are properly linked

### Runtime Issues

#### Application won't start

1. **Check dependencies**:
   ```bash
   # macOS / Linux
   ldd build/ModAI | grep Qt
   
   # Windows
   dumpbin /dependents build\ModAI.exe
   ```

2. **Check logs**: `data/logs/system.log`

3. **Verify API keys**: Check environment variables or config file

#### API authentication fails

1. **Verify API keys**: Check they're set correctly
2. **Check network**: Ensure internet connection
3. **Review logs**: Check `data/logs/system.log` for errors
4. **Test API keys**: Try using them with curl/Postman

#### Reddit scraping fails

1. **Verify OAuth credentials**: Check Client ID and Secret
2. **Check rate limits**: Reddit allows 60 requests/minute
3. **User agent**: Ensure user agent is set correctly
4. **Subreddit name**: Verify subreddit exists and is accessible

#### No items appearing in dashboard

1. **Check scraper status**: Look for "Scraping" status in UI
2. **Verify subreddit**: Ensure subreddit name is correct
3. **Check logs**: Review `data/logs/system.log` for errors
4. **API limits**: May have hit rate limits

---

## Platform-Specific Notes

### macOS

- **Homebrew**: Recommended package manager
- **Xcode**: May need Xcode Command Line Tools
- **Qt6**: Install via `brew install qt6`
- **Paths**: Qt6 typically at `/opt/homebrew/lib/cmake/Qt6` (Apple Silicon) or `/usr/local/lib/cmake/Qt6` (Intel)

### Linux

- **Package Managers**: Use `apt-get` (Debian/Ubuntu) or `yum` (RHEL/Fedora)
- **Qt6**: May need to add repositories for Qt6 packages
- **Paths**: Qt6 typically at `/usr/lib/cmake/Qt6` or `/usr/local/lib/cmake/Qt6`

### Windows

- **Visual Studio**: Required for MSVC compiler (or use MinGW)
- **Qt6**: Must be installed manually from qt.io
- **Paths**: Qt6 typically at `C:\Qt\6.x.x\msvc2019_64` or `C:\Qt\6.x.x\mingw_64`
- **CMake**: Can use GUI version for easier configuration
- **PowerShell**: Use PowerShell 5.1+ for scripts

---

## Next Steps

After setup:

1. **Configure Rules**: Edit `config/rules.json` for your moderation policies
2. **Test Scraping**: Start with a small subreddit to test
3. **Review Items**: Use the dashboard to review flagged content
4. **Export Reports**: Use export functionality to generate reports
5. **Customize UI**: Modify UI components as needed

---

## Getting Help

- **Logs**: Check `data/logs/system.log` for detailed error messages
- **Documentation**: See `README.md` and `PROJECT_SUMMARY.md`
- **Build Issues**: See `BUILD.md` for detailed build instructions
- **Specification**: See `spec.txt` for complete project specification

---

## Security Notes

**Important**: 
- API keys are currently stored in plain text in config files
- Do not commit API keys to version control
- Use environment variables in production
- Encryption will be added in a future version

---

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review logs in `data/logs/system.log`
3. Verify all dependencies are installed correctly
4. Ensure API keys are valid and have proper permissions

