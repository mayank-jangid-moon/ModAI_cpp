# Migration Notes: Qt to Core C++

## Overview
This migration decouples the core application logic from the Qt framework, enabling a headless, lightweight C++ backend that can run on servers without a windowing system.

## Key Changes

### 1. Dependency Removal
*   **Removed**: `Qt6::Core`, `Qt6::Network`, `Qt6::Widgets` from the core backend.
*   **Added**: `libcurl` (Network), `httplib` (Server), `std::thread` (Concurrency), `std::filesystem` (Storage).

### 2. Architecture Split
*   **`/core`**: Pure C++ logic. No Qt headers allowed.
*   **`/network`**: New `CurlHttpClient` replaces `QtHttpClient`. New `CppHttpServer` replaces `QTcpServer`.
*   **`/scraper`**: `RedditScraper` now uses `StdScheduler` instead of `QTimer`.

### 3. New Components
*   **`CoreController`**: The new entry point for the headless server. Initializes the stack without `QApplication`.
*   **`StdScheduler`**: A simple thread-based scheduler for periodic tasks (scraping).
*   **`CurlHttpClient`**: A synchronous HTTP client using `libcurl`.

## Build Instructions

### Prerequisites
*   CMake 3.20+
*   C++17 Compiler
*   `libcurl`, `openssl`, `nlohmann_json`

### Building the Core Server
```bash
mkdir build_core
cd build_core
cmake -f ../CMakeLists_Core.txt ..
make
./modai-server
```

## Verification
The new server exposes the same API on port 8080.
*   `GET /api/status`
*   `GET /api/items`
*   `POST /api/scraper/start`

The React frontend can connect to this server seamlessly.
