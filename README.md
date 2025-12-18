# ModAI - Trust & Safety Dashboard

A polished Qt-based desktop application (C++) that scrapes subreddit posts/comments, runs AI-detection and image/text moderation, and presents a real-time moderation dashboard with "railguard" blocking and human-review flow.

## Features

- **Subreddit Scraper**: Configurable subreddit scraping with Reddit public API support. Fetches both posts and comments.
- **Local AI Text Detection**: Uses local ONNX inference with `desklib/ai-text-detector-v1.01` model for fast, offline AI content detection
- **Image Moderation**: Hive/TheHive.ai visual moderation for NSFW, violence, hate, and drugs detection. Automatically downloads and processes images.
- **Text Moderation**: Hive text moderation for offensive/hate/abuse detection
- **Railguard**: Real-time blocking with animated overlay notifications
- **Dashboard**: Sortable table with search, status filters, visual badges, and detail panels
- **Rule Engine**: JSON-based configurable thresholds and actions
- **Export/Import**: CSV and JSON export/import functionality, PDF reports with images
- **Performance**: Structured retries for network resilience
- **Secure Storage**: Encrypted API key storage with environment variable support

## Architecture

The project follows a modular OOP design:

- `ui/` - Qt frontend (MainWindow, Dashboard, DetailPanel, RailguardOverlay)
- `core/` - Business logic (ModerationEngine, RuleEngine)
- `network/` - HTTP clients with rate limiting (QtNetwork-based)
- `detectors/` - AI detection and moderation interfaces (LocalAIDetector with ONNX Runtime, HiveImageModerator, HiveTextModerator)
- `scraper/` - RedditScraper with OAuth and rate limiting
- `storage/` - JSONL-based append-only storage (thread-safe, crash-safe)
- `export/` - PDF/CSV/JSON export and CSV/JSON import
- `utils/` - Logging, crypto, and helper utilities

## Quick Start

**For detailed setup instructions, see [SETUP.md](SETUP.md)**

### Quick Setup (macOS/Linux)

```bash
# 1. Install dependencies
./scripts/install_dependencies.sh

# 2. Export ONNX model (one-time setup)
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/ModAI/data/models

# 3. Set API keys (optional - only needed for Hive moderation)
export MODAI_HIVE_API_KEY="your_key"

# 4. Build
./scripts/build.sh

# 5. Run
./scripts/run.sh
```

### Quick Setup (Windows)

```powershell
# 1. Install dependencies
.\scripts\install_dependencies.ps1

# 2. Export ONNX model (one-time setup)
python3 scripts/export_model_to_onnx.py --output $env:LOCALAPPDATA\ModAI\ModAI\data\models

# 3. Set API keys (optional - only needed for Hive moderation)
$env:MODAI_HIVE_API_KEY = "your_key"

# 4. Build
.\scripts\build.ps1

# 5. Run
.\scripts\run.ps1
```

## Prerequisites

- **C++17 or C++20** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.20+**
- **Qt 6** (Widgets, Network, Charts)
- **nlohmann/json** (JSON parsing)
- **OpenSSL** (for TLS/HTTPS)
- **ONNX Runtime 1.16+** (for local AI inference)
- **Python 3.8+** (for model export, with torch, transformers, onnx packages)

**See [SETUP.md](SETUP.md) for detailed installation instructions and [LOCAL_AI_SETUP.md](docs/LOCAL_AI_SETUP.md) for model setup.**

## Building

### Using Scripts (Recommended)

```bash
# macOS/Linux
./scripts/build.sh

# Windows
.\scripts\build.ps1
```

### Manual Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The executable will be in `build/ModAI` (or `build/ModAI.exe` on Windows).

## Configuration

### API Keys

The application requires API keys for:
1. **Hive/TheHive.ai**: API key for visual and text moderation (optional)
2. **Reddit**: OAuth client ID and secret (optional, for authenticated scraping)

**Note**: AI text detection now uses local ONNX inference and does not require any API keys.

#### Setting API Keys (Optional)

**Option 1: Environment Variables**
```bash
export MODAI_HIVE_API_KEY="your_hive_key"
# Reddit OAuth (optional - for authenticated scraping)
export MODAI_REDDIT_CLIENT_ID="your_reddit_client_id"
export MODAI_REDDIT_CLIENT_SECRET="your_reddit_client_secret"
```

**Option 2: Configuration File**
The application will create a config file at:
- macOS: `~/Library/Preferences/ModAI/config.ini`
- Linux: `~/.config/ModAI/config.ini`
- Windows: `%APPDATA%/ModAI/config.ini`

You can manually edit this file or use the Settings UI (to be implemented).

### Rules Configuration

Edit `config/rules.json` to configure moderation rules. Example:

```json
{
  "rules": [
    {
      "id": "high_ai_score",
      "name": "High AI Score Block",
      "condition": "ai_score > 0.8",
      "action": "block",
      "subreddit": "",
      "enabled": true
    }
  ]
}
```

Supported conditions:
- `ai_score > 0.8` - AI detection score threshold
- `sexual > 0.9` - NSFW content threshold
- `violence > 0.9` - Violence content threshold
- `hate > 0.7` - Hate speech threshold
- `drugs > 0.8` - Drug-related content threshold

Actions: `allow`, `block`, `review`

## Usage

1. **Start the application**: Run the compiled executable
2. **Configure API keys**: Set environment variables or edit config file
3. **Enter subreddit**: Type a subreddit name (e.g., "technology")
4. **Start scraping**: Click "Start Scraping" button
5. **Review items**: Items are automatically analyzed and displayed in the dashboard
6. **Handle blocked content**: Railguard overlay appears for auto-blocked items
7. **Export reports**: Use export functionality to generate PDF/CSV/JSON reports

## Data Storage

The application uses **JSONL (JSON Lines)** format for storage:

- `data/content.jsonl` - All scraped and analyzed content
- `data/actions.jsonl` - Human moderation actions
- `data/logs/` - System logs
- `data/exports/` - Exported reports

All storage is **append-only**, **thread-safe**, and **crash-safe**. Data is human-readable and can be easily exported or migrated.

## API Endpoints Used

### Local ONNX Inference
- **Model**: desklib/ai-text-detector-v1.01 (DeBERTa-based)
- **Runtime**: ONNX Runtime (local, no API calls)
- **Performance**: ~100-500ms per text depending on length
- **Rate Limit**: None (runs locally)

### Hive/TheHive.ai
- **Visual Moderation**: `https://api.thehive.ai/api/v2/task/sync`
- **Text Moderation**: `https://api.thehive.ai/api/v1/moderation/text`
- **Method**: POST
- **Auth**: Bearer token
- **Rate Limit**: 100 requests per minute

### Reddit API
- **OAuth Token**: `https://www.reddit.com/api/v1/access_token`
- **Posts**: `https://oauth.reddit.com/r/{subreddit}/new.json`
- **Rate Limit**: 60 requests per minute

## Development

### Project Structure
```
ModAI_cpp/
├── CMakeLists.txt
├── README.md
├── config/
│   └── rules.json
├── include/
│   ├── core/
│   ├── detectors/
│   ├── export/
│   ├── network/
│   ├── scraper/
│   ├── storage/
│   ├── ui/
│   └── utils/
└── src/
    ├── main.cpp
    └── [same structure as include/]
```

### Running Tests

Tests are optional and can be added in the `tests/` directory. Configure with CMake:

```bash
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest
```

## Troubleshooting

### Build Issues

**Qt6 not found:**
```bash
# Set Qt6_DIR
export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6
```

**nlohmann/json not found:**
```bash
# Install via package manager or download header-only version
```

### Runtime Issues

**API authentication fails:**
- Verify API keys are set correctly
- Check network connectivity
- Review logs in `data/logs/system.log`

**Reddit scraping fails:**
- Verify Reddit OAuth credentials
- Check rate limits (60 requests/minute)
- Ensure user agent is set correctly

## License

This project is provided as-is for educational and development purposes.

## Contributing

This is a reference implementation following the specification. For production use, consider:
- Adding proper encryption for API keys
- Implementing full PDF export with Qt's QPrinter
- Adding comprehensive unit tests
- Implementing proper error recovery and retry logic
- Adding UI for settings and configuration

## Acknowledgments

- **Hugging Face** for the AI text detection model
- **TheHive.ai** for moderation APIs
- **Reddit** for API access
- **Qt** framework for the UI
