# Migration Notes

## Overview
This document details the migration of the ModAI frontend from Qt Widgets to a React-based web application. The backend logic remains in C++ but is now exposed via a lightweight HTTP server.

## Architecture Changes

### Backend
*   **New Entry Point**: `AppController` replaces `MainWindow` as the central orchestrator. It manages the lifecycle of `ModerationEngine`, `RedditScraper`, and `Storage` without any UI dependencies.
*   **HTTP Server**: A custom `HttpServer` (based on `QTcpServer`) has been added to `src/network/`. It runs on port 8080 and handles JSON API requests.
*   **Main Loop**: `src/main.cpp` has been modified to support a `--server` flag. When present, it initializes `QCoreApplication` (headless) instead of `QApplication` (GUI).

### Frontend
*   **Framework**: React (Vite) + Tailwind CSS.
*   **Communication**: Fetch API calls to `http://localhost:8080/api`.
*   **State Management**: Local component state (useState/useEffect). No global state library was introduced to keep complexity low.

## API Design
The API follows a REST-like structure using JSON.
*   **Status/Control**: `/api/status`, `/api/scraper/start`, `/api/scraper/stop`
*   **Data Access**: `/api/items` (paginated, filtered), `/api/items/:id`
*   **Actions**: `/api/items/:id/decision` (POST)

## Trade-offs & Decisions
1.  **QTcpServer vs Crow/Boost**: I chose `QTcpServer` to minimize external dependencies. Since the project already links against Qt Network, this avoided adding new libraries to the build system.
2.  **Polling vs WebSockets**: The frontend currently polls `/api/items` every 5 seconds. For a production system, WebSockets (via `QWebSocketServer`) would be more efficient for real-time updates.
3.  **Data Consistency**: The backend uses a simple file-based storage (`JsonlStorage`). Concurrent writes are handled by the single-threaded nature of the Qt event loop (signals/slots), ensuring thread safety without complex locking.

## Running the New System

### 1. Start the Backend
```bash
./build/ModAI --server
```

### 2. Start the Frontend
```bash
cd frontend
npm install
npm run dev
```

## Future Work
*   Implement authentication for the API.
*   Add WebSocket support for real-time item updates.
*   Improve error handling in the HTTP server (better parsing).
*   Add unit tests for `AppController`.
