# Rewrite Analysis: Qt Decoupling

## 1. Overview
The goal is to extract the core application logic from the Qt framework to create a portable, framework-agnostic C++ backend. Currently, the backend logic is tightly coupled with Qt's event loop, signals/slots, and data types.

## 2. Logic Analysis & Classification

### A. Pure UI (Qt-Only) -> Stays in Frontend
*   `MainWindow` (and `.ui` files if any)
*   `DashboardModel`, `DashboardProxyModel` (Qt Item Models)
*   `DetailPanel`, `RailguardOverlay`, `ReviewDialog`
*   `AppController` (Currently acts as a bridge, but heavily relies on Qt types)

### B. Application Logic (Portable C++) -> Must Be Extracted
*   **`ModerationEngine`**: Core orchestration logic. Currently uses `std::function` callbacks (good), but dependencies might leak Qt.
*   **`RuleEngine`**: Pure logic for evaluating rules. Likely safe.
*   **`ContentItem`**: Data model. Currently has `Q_DECLARE_METATYPE` and uses `std::string` (mostly safe, but needs to drop Qt meta-type registration).
*   **`ResultCache`**: Caching logic.
*   **`RateLimiter`**: Logic for rate limiting.

### C. Infrastructure Glue -> Needs Redesign
*   **`RedditScraper`**:
    *   *Current*: Inherits `QObject`, uses `QTimer` for scheduling, `QtHttpClient` for network.
    *   *Target*: Plain C++ class using a portable `Scheduler` interface and `HttpClient` interface.
*   **`QtHttpClient`**:
    *   *Current*: Implements `HttpClient` interface but uses `QNetworkAccessManager`.
    *   *Target*: Keep as an adapter for the Qt frontend, but create a new `CurlHttpClient` or `CppHttpClient` (using `cpr` or `httplib`) for the core backend.
*   **`Storage` / `JsonlStorage`**:
    *   *Current*: Likely uses `QFile` / `QDir` internally.
    *   *Target*: Use `std::filesystem` and `std::fstream`.
*   **`Logger`**:
    *   *Current*: Likely uses `QDebug` or `QFile`.
    *   *Target*: Use `spdlog` or `std::cout` / `std::fstream`.

## 3. Qt Dependencies to Eliminate in `/core`
*   `#include <QObject>`
*   `#include <QTimer>`
*   `#include <QMetaType>`
*   `#include <QString>` (if used in implementation files)
*   `#include <QFile>`, `<QDir>`
*   `#include <QNetworkAccessManager>`

## 4. Extraction Strategy

### Phase 1: Interfaces & Data Models
1.  **`ContentItem`**: Remove `Q_DECLARE_METATYPE`. Ensure all types are std types.
2.  **`HttpClient`**: The interface is already pure C++. This is good.
3.  **`Storage`**: Ensure interface uses std types.

### Phase 2: Core Logic
1.  **`ModerationEngine`**: Verify no Qt dependencies.
2.  **`RuleEngine`**: Verify no Qt dependencies.

### Phase 3: Infrastructure Replacement
1.  **`RedditScraper`**:
    *   Remove `QObject` inheritance.
    *   Replace `QTimer` with a `std::thread` loop or a `Scheduler` dependency.
    *   Inject `HttpClient` (already done, but need a non-Qt implementation).
2.  **`JsonlStorage`**: Rewrite file I/O using `std::filesystem`.

### Phase 4: Network Layer
1.  **New `StdHttpClient`**: Implement `HttpClient` using a lightweight C++ library (e.g., `httplib` or `libcurl`). *Constraint check: "Compile with zero Qt dependencies"*.
    *   *Decision*: Since we want to avoid adding heavy dependencies if possible, we might need a simple wrapper around `libcurl` if available, or just use `httplib` (header-only) for the server and client.
    *   *Actually*, the prompt says "Qt becomes just another client". The backend needs to make HTTP requests (to Reddit/Hive).
    *   *Action*: Create `CurlHttpClient` or similar.

## 5. Risks & Edge Cases
*   **Event Loop**: `RedditScraper` relies on `QTimer` and the Qt event loop for async execution. Moving to `std::thread` requires careful management of thread safety and lifecycle (stopping the scraper cleanly).
*   **Thread Safety**: Qt's signal/slot mechanism handles cross-thread communication automatically (QueuedConnection). We must manually handle mutexes/locks when moving to pure C++.
*   **Unicode**: `QString` handles Unicode well. `std::string` (UTF-8) is generally fine but file paths on Windows can be tricky. We are on macOS, so `std::string` + `std::filesystem` is safe.

## 6. Plan
1.  Create `core/interfaces` for `HttpClient`, `Storage`, `Scheduler`.
2.  Refactor `ContentItem` to be pure C++.
3.  Refactor `RedditScraper` to use `std::thread` and `std::chrono`.
4.  Refactor `JsonlStorage` to use `std::filesystem`.
5.  Create `CoreHttpClient` (using `httplib` or `curl`).
6.  Create `CoreServer` (using `httplib` for the API).

