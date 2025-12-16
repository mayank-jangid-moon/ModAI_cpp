# Backend Analysis & Capability Map

## 1. System Overview
The current application is a Qt-based desktop application (`ModAI`) that performs content moderation on Reddit posts. It uses a pipeline architecture:
1.  **Scraper**: Fetches content from Reddit.
2.  **Moderation Engine**: Orchestrates analysis using multiple detectors.
3.  **Detectors**:
    *   `LocalAIDetector`: ONNX-based text analysis.
    *   `HiveImageModerator`: External API for image moderation.
    *   `HiveTextModerator`: External API for text moderation.
4.  **Rule Engine**: Applies rules to analysis results to make a decision (Allow/Block/Review).
5.  **Storage**: JSONL-based persistence for content and actions.
6.  **UI**: Qt `MainWindow` for control and visualization.

## 2. Backend Entry Points & Components
The backend logic is encapsulated in the following classes, currently instantiated and managed by `MainWindow`:

*   **`ModAI::ModerationEngine`**: The core processor.
    *   **Input**: `ContentItem`
    *   **Output**: Processed `ContentItem` (via callback)
    *   **Dependencies**: Detectors, RuleEngine, Storage, Cache.
*   **`ModAI::RedditScraper`**: Data source.
    *   **Input**: Subreddit name, frequency.
    *   **Output**: `ContentItem` (via callback).
*   **`ModAI::Storage` / `ModAI::JsonlStorage`**: Persistence.
    *   **Methods**: `saveContent`, `saveAction`, `loadAllContent`, `loadAllActions`.
*   **`ModAI::QtHttpClient`**: Network layer.
    *   **Dependency**: Requires Qt Event Loop.

## 3. Data Models
*   **`ContentItem`**:
    *   `id` (string)
    *   `timestamp` (ISO-8601 string)
    *   `source` (string)
    *   `subreddit` (string)
    *   `content_type` ("text" | "image")
    *   `text` (optional string)
    *   `image_path` (optional string)
    *   `ai_detection` (struct: score, label, confidence)
    *   `moderation` (struct: labels for sexual, violence, etc.)
    *   `decision` (struct: auto_action, rule_id)
*   **`HumanAction`**:
    *   `action_id` (string)
    *   `content_id` (string)
    *   `new_status` (string)
    *   `reviewer` (string)
    *   `reason` (string)

## 4. Workflows & State Transitions

### A. Scraping Workflow
1.  **Trigger**: User starts scraping a subreddit.
2.  **Action**: `RedditScraper` fetches posts.
3.  **Flow**: `scraper` -> `onItemScraped` -> `ModerationEngine::processItem`.
4.  **Processing**:
    *   Detectors run (async/sync mix).
    *   `RuleEngine` evaluates results.
    *   `Storage` saves the item.
    *   `ResultCache` caches results.
5.  **Completion**: `ModerationEngine` calls `onItemProcessed` callback.

### B. Review/Override Workflow
1.  **Trigger**: User overrides a decision in UI.
2.  **Action**: Create `HumanAction`.
3.  **Persistence**: `Storage::saveAction`.
4.  **State Update**: Item status updated in memory/UI.

### C. Initialization
1.  Load API keys (`Crypto`).
2.  Initialize Detectors (ONNX, Hive).
3.  Load Rules (`rules.json`).
4.  Initialize Storage & Cache.

## 5. Backend Assumptions & Constraints
*   **Qt Dependency**: The backend heavily relies on Qt (`QString`, `QThread`, `QTimer`, `QtHttpClient`, signals/slots).
*   **Event Loop**: `QtHttpClient` and `QTimer` require a running `QEventLoop`.
*   **File System**: Relies on `QStandardPaths::AppDataLocation` for data, models, and logs.
*   **Threading**: `ModerationEngine` and `RedditScraper` operate asynchronously but interact with the main thread via callbacks/signals.

## 6. Capability Map (Proposed API)

| Capability | Input | Output | Side Effects |
| :--- | :--- | :--- | :--- |
| **Get Status** | None | `{ "scraping": bool, "queue_size": int, "subreddit": string }` | None |
| **Start Scraping** | `{ "subreddit": string }` | `{ "status": "started" }` | Starts scraper, updates state. |
| **Stop Scraping** | None | `{ "status": "stopped" }` | Stops scraper. |
| **Get Items** | Query params (limit, offset, filter) | `[ ContentItem ]` | Reads from Storage/Memory. |
| **Get Item Details** | `id` | `ContentItem` | Reads from Storage. |
| **Submit Action** | `{ "content_id": string, "action": "allow"\|"block", "reason": string }` | `HumanAction` | Writes to Storage, updates Item. |
| **Get Statistics** | None | `{ "total": int, "flagged": int, "reviewed": int }` | Aggregates data. |

## 7. Migration Risks
*   **Qt Event Loop Integration**: The new HTTP server must run without blocking the Qt event loop, or run in a separate thread while safely posting events to the main Qt thread.
*   **Thread Safety**: Accessing shared state (like the list of items) from the HTTP server thread and the Qt worker threads needs synchronization.
