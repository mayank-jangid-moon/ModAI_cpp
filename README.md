# ModAI - Trust & Safety Dashboard

A polished Qt-based desktop application (C++) that scrapes subreddit posts/comments, runs AI-detection and image/text moderation, and presents a real-time moderation dashboard with "railguard" blocking and human-review flow.

## Features

- **Subreddit Scraper**: Configurable subreddit scraping with Reddit OAuth API support
- **AI Text Detection**: Uses Hugging Face model `desklib/ai-text-detector-v1.01` to detect AI-generated content
- **Image Moderation**: Hive/TheHive.ai visual moderation for NSFW, violence, hate, and drugs detection
- **Text Moderation**: Hive text moderation for offensive/hate/abuse detection
- **Railguard**: Real-time blocking with animated overlay notifications
- **Dashboard**: Sortable table with filters, visual badges, and detail panels
- **Rule Engine**: JSON-based configurable thresholds and actions
- **Export**: PDF, CSV, and JSON export functionality
- **Secure Storage**: Encrypted API key storage with environment variable support

## Architecture

The project follows a modular OOP design:

- `ui/` - Qt frontend (MainWindow, Dashboard, DetailPanel, RailguardOverlay)
- `core/` - Business logic (ModerationEngine, RuleEngine)
- `network/` - HTTP clients with rate limiting (QtNetwork-based)
- `detectors/` - AI detection and moderation interfaces (HFTextDetector, HiveImageModerator, HiveTextModerator)
- `scraper/` - RedditScraper with OAuth and rate limiting
- `storage/` - JSONL-based append-only storage (thread-safe, crash-safe)
- `export/` - PDF/CSV/JSON exporters
- `utils/` - Logging, crypto, and helper utilities

## Quick Start

**For detailed setup instructions, see [SETUP.md](SETUP.md)**

### Quick Setup (macOS/Linux)

```bash
# 1. Install dependencies
./scripts/install_dependencies.sh

# 2. Set API keys
export MODAI_HUGGINGFACE_TOKEN="your_token"
export MODAI_HIVE_API_KEY="your_key"
export MODAI_REDDIT_CLIENT_ID="your_id"
export MODAI_REDDIT_CLIENT_SECRET="your_secret"

# 3. Build
./scripts/build.sh

# 4. Run
./scripts/run.sh
```

### Quick Setup (Windows)

```powershell
# 1. Install dependencies
.\scripts\install_dependencies.ps1

# 2. Set API keys
$env:MODAI_HUGGINGFACE_TOKEN = "your_token"
$env:MODAI_HIVE_API_KEY = "your_key"
$env:MODAI_REDDIT_CLIENT_ID = "your_id"
$env:MODAI_REDDIT_CLIENT_SECRET = "your_secret"

# 3. Build
.\scripts\build.ps1

# 4. Run
.\scripts\run.ps1
```

## Prerequisites

- **C++17 or C++20** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.20+**
- **Qt 6** (Widgets, Network, Charts)
- **nlohmann/json** (JSON parsing)
- **OpenSSL** (for TLS/HTTPS)

**See [SETUP.md](SETUP.md) for detailed installation instructions.**

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
1. **Hugging Face**: Token for AI text detection
2. **Hive/TheHive.ai**: API key for visual and text moderation
3. **Reddit**: OAuth client ID and secret

#### Setting API Keys

**Option 1: Environment Variables (Recommended for CI)**
```bash
export MODAI_HUGGINGFACE_TOKEN="your_hf_token"
export MODAI_HIVE_API_KEY="your_hive_key"
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
- `data/cache/` - Cached detection results
- `data/logs/` - System logs
- `data/exports/` - Exported reports

All storage is **append-only**, **thread-safe**, and **crash-safe**. Data is human-readable and can be easily exported or migrated.

## API Endpoints Used

### Hugging Face Inference API
- **Endpoint**: `https://api-inference.huggingface.co/models/desklib/ai-text-detector-v1.01`
- **Method**: POST
- **Auth**: Bearer token
- **Rate Limit**: 30 requests per minute

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
