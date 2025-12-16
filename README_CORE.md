# ModAI Core Server

This is the Qt-free, headless version of the ModAI backend.

## Dependencies

- CMake 3.20+
- C++17 compiler
- libcurl
- OpenSSL
- nlohmann_json (header-only, included in `backend/include`)
- cpp-httplib (header-only, included in `include/httplib.h`)

## Build Instructions

```bash
mkdir build_core
cd build_core
cmake ..
make
```

## Running

```bash
./modai-server
```

## Configuration

The server looks for configuration and data in `~/.local/share/ModAI/data`.

Environment Variables:
- `MODAI_HIVE_API_KEY`: API key for Hive moderation.
- `MODAI_REDDIT_CLIENT_ID`: Reddit App Client ID.
- `MODAI_REDDIT_CLIENT_SECRET`: Reddit App Client Secret.

## API Endpoints

- `GET /api/status`: Check server status.
- `POST /api/scraper/start`: Start scraping (Body: `{"subreddit": "name"}`).
- `POST /api/scraper/stop`: Stop scraping.
- `GET /api/items`: Retrieve processed items.

## Notes

- PDF export is disabled in this version.
- Local AI detection requires ONNX Runtime (optional).
