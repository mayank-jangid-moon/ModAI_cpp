# ModAI - Trust & Safety Dashboard

A professional Qt-based desktop application written in C++ that provides comprehensive content moderation capabilities including Reddit scraping, AI-powered chatbot functionality, AI-generated content detection, and real-time moderation workflows.

## Features

### Content Moderation
- **Subreddit Scraper**: Configurable subreddit scraping with Reddit public API support for posts and comments
- **Local AI Text Detection**: ONNX-based inference using the desklib/ai-text-detector-v1.01 model for fast, offline AI content detection
- **Image Moderation**: Hive/TheHive.ai visual moderation for NSFW, violence, hate speech, and drug-related content
- **Text Moderation**: Hive text moderation for offensive language, hate speech, and abusive content detection
- **Rule Engine**: JSON-based configurable thresholds and automated action triggers

### AI Integration
- **AI Chatbot with Content Filtering**: Real-time chat with LLM integration and automatic response moderation
- **AI Text Detector**: Standalone tool for detecting AI-generated text content
- **AI Image Detector**: Visual analysis for identifying AI-generated images

### Workflow Management
- **Real-time Dashboard**: Sortable table with search functionality, status filters, and visual indicators
- **Human Review Flow**: Manual override capabilities with detailed analysis panels
- **Export/Import**: Comprehensive data export in CSV, JSON, and PDF formats with image embedding
- **Secure Storage**: Encrypted API key storage with environment variable support

## Architecture

The application follows a modular object-oriented design:

- **ui/** - Qt-based user interface components (MainWindow, Panels, Dialogs)
- **core/** - Business logic layer (ModerationEngine, RuleEngine)
- **network/** - HTTP client implementation with rate limiting (Qt Network-based)
- **detectors/** - AI detection and moderation interfaces (LocalAIDetector with ONNX Runtime, Hive moderators)
- **scraper/** - RedditScraper with OAuth support and rate limiting
- **storage/** - JSONL-based append-only storage system (thread-safe, crash-resistant)
- **export/** - Data export functionality (PDF, CSV, JSON) and import capabilities
- **utils/** - Logging, cryptography, and utility functions

For detailed architectural information, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Quick Start

For comprehensive setup instructions, refer to [SETUP.md](SETUP.md).

### Setup Instructions (macOS/Linux)

```bash
# 1. Install dependencies
./scripts/install_dependencies.sh

# 2. Export ONNX model (one-time setup)
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/ModAI/data/models

# 3. Configure API keys (optional - required only for Hive moderation)
export MODAI_HIVE_API_KEY="your_key"

# 4. Build the application
./scripts/build.sh

# 5. Run the application
./scripts/run.sh
```

### Setup Instructions (Windows)

```powershell
# 1. Install dependencies
.\scripts\install_dependencies.ps1

# 2. Export ONNX model (one-time setup)
python3 scripts/export_model_to_onnx.py --output $env:LOCALAPPDATA\ModAI\ModAI\data\models

# 3. Configure API keys (optional - required only for Hive moderation)
$env:MODAI_HIVE_API_KEY = "your_key"

# 4. Build the application
.\scripts\build.ps1

# 5. Run the application
.\scripts\run.ps1
```

## System Requirements

- **C++ Compiler**: C++17 or C++20 compatible (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake**: Version 3.20 or higher
- **Qt 6**: Including Widgets, Network, and Charts modules
- **nlohmann/json**: JSON parsing library
- **OpenSSL**: For TLS/HTTPS support
- **ONNX Runtime**: Version 1.16 or higher (for local AI inference)
- **Python**: Version 3.8 or higher (for model export, requires torch, transformers, onnx packages)

Refer to [SETUP.md](SETUP.md) for detailed installation instructions and [docs/LOCAL_AI_SETUP.md](docs/LOCAL_AI_SETUP.md) for model configuration.

Refer to [SETUP.md](SETUP.md) for detailed installation instructions and [docs/LOCAL_AI_SETUP.md](docs/LOCAL_AI_SETUP.md) for model configuration.

## Building

### Using Build Scripts (Recommended)

```bash
# macOS/Linux
./scripts/build.sh

# Windows
.\scripts\build.ps1
```

### Manual Build Process

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The compiled executable will be located at `build/ModAI` (or `build/ModAI.exe` on Windows).

## Configuration

### API Keys

The application supports optional API keys for extended functionality:

1. **Hive/TheHive.ai**: API key for visual and text moderation (optional)
2. **Reddit**: OAuth client ID and secret (optional, for authenticated scraping)

Note: AI text detection uses local ONNX inference and does not require API keys.

#### Configuring API Keys (Optional)

**Option 1: Environment Variables**
```bash
export MODAI_HIVE_API_KEY="your_hive_key"
# Reddit OAuth (optional - for authenticated scraping)
export MODAI_REDDIT_CLIENT_ID="your_reddit_client_id"
export MODAI_REDDIT_CLIENT_SECRET="your_reddit_client_secret"
```

**Option 2: Configuration File**
The application automatically creates a configuration file at:
- macOS: `~/Library/Preferences/ModAI/config.ini`
- Linux: `~/.config/ModAI/config.ini`
- Windows: `%APPDATA%/ModAI/config.ini`

You can manually edit this file or use the application's settings interface.

### Rules Configuration

Moderation rules can be configured by editing `config/rules.json`. Example configuration:

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

Supported condition types:
- `ai_score > 0.8` - AI detection score threshold
- `sexual > 0.9` - NSFW content threshold
- `violence > 0.9` - Violence content threshold
- `hate > 0.7` - Hate speech threshold
- `drugs > 0.8` - Drug-related content threshold

Available actions: `allow`, `block`, `review`

## Usage

1. Launch the application by running the compiled executable
2. Configure API keys through environment variables or the configuration file
3. Navigate to the desired mode using the tab interface
4. For Reddit scraping: Enter a subreddit name and click "Start Scraping"
5. Review analyzed content in the dashboard with automated filtering
6. Handle flagged content using the review workflow
7. Export results using the built-in export functionality (PDF/CSV/JSON)

## Data Storage

The application utilizes JSONL (JSON Lines) format for persistent storage:

- `data/content.jsonl` - All scraped and analyzed content items
- `data/actions.jsonl` - Human moderation actions and decisions
- `data/logs/` - System and application logs
- `data/exports/` - Generated export files

All storage operations are append-only, thread-safe, and crash-resistant. Data is stored in human-readable format for easy inspection and migration.

## API Integration

### Local ONNX Inference
- **Model**: desklib/ai-text-detector-v1.01 (DeBERTa-based transformer)
- **Runtime**: ONNX Runtime (local execution, no external API calls)
- **Performance**: 100-500ms per text sample depending on length
- **Rate Limit**: None (local execution)

### Hive/TheHive.ai APIs
- **Visual Moderation**: `https://api.thehive.ai/api/v2/task/sync`
- **Text Moderation**: `https://api.thehive.ai/api/v1/moderation/text`
- **Method**: POST
- **Authentication**: Bearer token
- **Rate Limit**: 100 requests per minute

### Reddit API
- **OAuth Token Endpoint**: `https://www.reddit.com/api/v1/access_token`
- **Content Endpoint**: `https://oauth.reddit.com/r/{subreddit}/new.json`
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
    └── [mirrors include/ structure]
```

### Running Tests

Test support can be added through the `tests/` directory. Configure with CMake:

```bash
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest
```

## Troubleshooting

### Build Issues

**Qt6 not found:**
```bash
# Set Qt6_DIR environment variable
export Qt6_DIR=/path/to/qt6/lib/cmake/Qt6
```

**nlohmann/json not found:**
Install via your package manager or download the header-only version from the official repository.

### Runtime Issues

**API authentication failures:**
- Verify API keys are configured correctly
- Check network connectivity
- Review application logs in `data/logs/system.log`

**Reddit scraping failures:**
- Verify Reddit OAuth credentials
- Check rate limits (60 requests/minute)
- Ensure user agent string is properly configured

## License

This project is provided as-is for educational and development purposes.

## Contributing

This is a reference implementation. For production deployment, consider:
- Implementing comprehensive encryption for API keys
- Adding complete PDF export functionality using Qt's QPrinter
- Developing thorough unit and integration test coverage
- Implementing robust error recovery and retry mechanisms
- Creating a user interface for settings and configuration management

## Acknowledgments

- **Hugging Face** for providing the AI text detection model
- **TheHive.ai** for moderation API services
- **Reddit** for API access
- **Qt Project** for the UI framework
