# ModAI C++ Backend

Pure C++ backend for ModAI content moderation system with REST API.

## Features

- **REST API Server** - HTTP API for content moderation
- **AI Text Detection** - Local ONNX-based AI text detection
- **Content Moderation** - Image and text moderation via Hive AI
- **Rule Engine** - Flexible rule-based decision making
- **Storage** - JSONL-based content and action storage
- **Export** - CSV and JSON export capabilities
- **No Qt Dependencies** - Pure C++ with standard libraries

## Architecture

The backend is organized into the following modules:

### Core
- `ContentItem` - Content representation and serialization
- `ModerationEngine` - Main processing pipeline
- `RuleEngine` - Rule evaluation and decision making
- `ResultCache` - Caching for API responses

### Detectors
- `LocalAIDetector` - Local AI text detection (ONNX)
- `HiveImageModerator` - Image moderation via Hive AI
- `HiveTextModerator` - Text moderation via Hive AI

### Network
- `CurlHttpClient` - HTTP client using libcurl
- `RateLimiter` - API rate limiting

### Storage
- `JsonlStorage` - JSONL file-based storage
- `Exporter` - Data export functionality

### API
- `ApiServer` - REST API server using cpp-httplib

## API Endpoints

### Health Check
```
GET /api/health
```

### Content Management
```
GET /api/content                    # List all content
GET /api/content/{id}               # Get specific content
PUT /api/content/{id}/decision      # Update decision (human review)
```

### Moderation
```
POST /api/moderate/text             # Moderate text content
POST /api/moderate/image            # Moderate image content
```

### Statistics
```
GET /api/stats                      # Get moderation statistics
```

### Export
```
GET /api/export?format=csv|json     # Export data
```

## Building

### Dependencies

- CMake 3.20+
- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- libcurl
- OpenSSL
- nlohmann/json
- cpp-httplib (auto-downloaded via FetchContent)
- ONNX Runtime (optional, for local AI detection)

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libcurl4-openssl-dev libssl-dev nlohmann-json3-dev
```

#### Fedora/RHEL
```bash
sudo dnf install -y cmake gcc-c++ libcurl-devel openssl-devel json-devel
```

#### macOS
```bash
brew install cmake curl openssl nlohmann-json
```

### Compile

```bash
cd backend
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Running

### Basic Usage

```bash
./ModAI_Backend
```

### With Options

```bash
./ModAI_Backend \
    --port 8080 \
    --data /path/to/data \
    --rules /path/to/rules.json \
    --model /path/to/model.onnx \
    --tokenizer /path/to/tokenizer
```

### Environment Variables

Set API keys via environment:

```bash
export MODAI_HIVE_API_KEY="your_hive_api_key"
./ModAI_Backend
```

## Configuration

### Rules Configuration

Create `config/rules.json`:

```json
{
  "rules": [
    {
      "id": "rule_001",
      "name": "High AI Score Block",
      "condition": "ai_score > 0.8",
      "action": "block",
      "subreddit": "",
      "enabled": true
    },
    {
      "id": "rule_002",
      "name": "Sexual Content Review",
      "condition": "sexual > 0.7",
      "action": "review",
      "subreddit": "",
      "enabled": true
    }
  ]
}
```

## API Examples

### Moderate Text

```bash
curl -X POST http://localhost:8080/api/moderate/text \
  -H "Content-Type: application/json" \
  -d '{
    "text": "This is a test message",
    "subreddit": "test",
    "author": "testuser"
  }'
```

### Moderate Image

```bash
curl -X POST http://localhost:8080/api/moderate/image \
  -F "image=@/path/to/image.jpg" \
  -F "subreddit=test"
```

### Get Statistics

```bash
curl http://localhost:8080/api/stats
```

### Export Data

```bash
curl "http://localhost:8080/api/export?format=csv"
curl "http://localhost:8080/api/export?format=json"
```

## Development

### Project Structure

```
backend/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── api/
│   ├── core/
│   ├── detectors/
│   ├── export/
│   ├── network/
│   ├── storage/
│   └── utils/
└── src/
    ├── api/
    ├── core/
    ├── detectors/
    ├── export/
    ├── network/
    ├── storage/
    ├── utils/
    └── main.cpp
```

### Adding New Endpoints

Edit `src/api/ApiServer.cpp` and add new routes:

```cpp
svr.Get("/api/your-endpoint", [](const httplib::Request& req, httplib::Response& res) {
    // Your handler code
});
```

## License

Same as parent project.
