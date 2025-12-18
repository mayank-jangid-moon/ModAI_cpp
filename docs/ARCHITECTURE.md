# ModAI System Architecture Documentation

## Executive Summary

ModAI is a comprehensive Qt6-based C++ desktop application designed for content moderation, AI-powered chatbot functionality, and content analysis. The system integrates multiple AI services, implements local ONNX-based detection, and provides a unified interface for content moderation workflows.

**Version:** 1.0  
**Language:** C++17  
**Framework:** Qt 6  
**Build System:** CMake  

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Architecture Layers](#architecture-layers)
3. [Component Structure](#component-structure)
4. [Data Flow Architecture](#data-flow-architecture)
5. [Class Hierarchy](#class-hierarchy)
6. [Module Interactions](#module-interactions)
7. [External Integrations](#external-integrations)
8. [Storage Architecture](#storage-architecture)
9. [Security Model](#security-model)
10. [Deployment Architecture](#deployment-architecture)

---

## 1. System Overview

### 1.1 Purpose
ModAI provides a unified platform for:
- **Content Moderation**: Automated analysis of text and images for inappropriate content
- **AI Chatbot**: Conversational AI with image generation capabilities
- **Content Analysis**: Local and cloud-based AI detection
- **Content Scraping**: Reddit content aggregation and moderation
- **Dashboard**: Real-time monitoring and decision-making interface

### 1.2 Key Features
- Multi-modal content moderation (text, images)
- Real-time AI-powered chat with image generation
- Local ONNX-based AI content detection
- Cloud-based moderation via Hive API
- Reddit content scraping and analysis
- Export/Import functionality for moderation results
- Rule-based content filtering

### 1.3 Technology Stack
```
┌─────────────────────────────────────┐
│      Presentation Layer (Qt6)       │
│   - MainWindow, Panels, Dialogs     │
├─────────────────────────────────────┤
│        Business Logic Layer         │
│   - Moderation Engine               │
│   - Rule Engine                     │
│   - AI Detectors                    │
├─────────────────────────────────────┤
│         Service Layer               │
│   - HTTP Client (Qt Network)        │
│   - API Integrations                │
│   - Local AI (ONNX Runtime)         │
├─────────────────────────────────────┤
│         Data Layer                  │
│   - JSONL Storage                   │
│   - File System                     │
└─────────────────────────────────────┘
```

---

## 2. Architecture Layers

### 2.1 Presentation Layer (UI)
**Location:** `include/ui/`, `src/ui/`

#### Components:
- **MainWindow** (`MainWindow.h/cpp`)
  - Central application window
  - Tab-based navigation
  - Menu system
  - Status bar management

- **ChatbotPanel** (`ChatbotPanel.h/cpp`)
  - Chat interface with AI
  - Image generation UI
  - Message history display
  - Image moderation integration
  - Show/Hide blocked content controls

- **AITextDetectorPanel** (`AITextDetectorPanel.h/cpp`)
  - Text analysis interface
  - Probability visualization
  - Batch text processing

- **AIImageDetectorPanel** (`AIImageDetectorPanel.h/cpp`)
  - Image upload and analysis
  - Visual moderation display
  - Drag-and-drop support

- **DetailPanel** (`DetailPanel.h/cpp`)
  - Content detail viewer
  - Moderation results display
  - Action controls (approve/reject)

- **ReviewDialog** (`ReviewDialog.h/cpp`)
  - Content review interface
  - Decision recording
  - Comment system

- **DashboardModel & DashboardProxyModel** (`DashboardModel.h/cpp`)
  - Table view data model
  - Sorting and filtering
  - Real-time updates

- **RailguardOverlay** (`RailguardOverlay.h/cpp`)
  - Visual feedback overlay
  - Content blocking indicators

### 2.2 Business Logic Layer

#### Core Engine (`include/core/`, `src/core/`)

**ModerationEngine** (`ModerationEngine.h/cpp`)
- Central orchestrator for content moderation
- Coordinates text and image detectors
- Manages moderation workflow
- Aggregates results from multiple sources

```cpp
class ModerationEngine {
    // Orchestrates all moderation activities
    ModerationResult moderateText(const std::string& text);
    ModerationResult moderateImage(const std::vector<uint8_t>& imageData);
    void setTextDetector(std::unique_ptr<TextDetector>);
    void setImageModerator(std::unique_ptr<ImageModerator>);
};
```

**RuleEngine** (`RuleEngine.h/cpp`)
- Rule-based content filtering
- Pattern matching
- Custom rule definitions
- Priority-based evaluation

```cpp
class RuleEngine {
    // Applies custom moderation rules
    void loadRules(const std::string& jsonPath);
    bool shouldBlock(const ContentItem& item);
    std::vector<std::string> matchedRules(const ContentItem& item);
};
```

**ContentItem** (`ContentItem.h`)
- Core data structure for moderated content
- Encapsulates content metadata
- Status tracking (pending, approved, rejected)

```cpp
struct ContentItem {
    std::string id;
    std::string content;
    ContentType type;  // TEXT, IMAGE, VIDEO
    ModerationStatus status;
    double aiScore;
    std::vector<std::string> flags;
    std::chrono::system_clock::time_point timestamp;
};
```

### 2.3 Service Layer

#### Network Services (`include/network/`, `src/network/`)

**HttpClient Interface** (`HttpClient.h`)
- Abstract HTTP client interface
- Request/response abstraction
- Retry logic
- Timeout management

**QtHttpClient** (`QtHttpClient.h/cpp`)
- Qt Network implementation
- Multipart form-data support
- JSON body encoding
- Base64 encoding for images
- Thread-safe network access

```cpp
class QtHttpClient : public HttpClient {
    HttpResponse get(const HttpRequest& req) override;
    HttpResponse post(const HttpRequest& req) override;
    HttpResponse put(const HttpRequest& req) override;
    
private:
    QNetworkAccessManager* getNetworkManager();
    QNetworkRequest createRequest(const std::string& url, 
                                  const std::map<std::string, std::string>& headers);
    HttpResponse executeRequest(QNetworkReply* reply);
};
```

**RateLimiter** (`RateLimiter.h/cpp`)
- Token bucket algorithm
- Per-API rate limiting
- Automatic request throttling
- Configurable limits

#### Detector Services (`include/detectors/`, `src/detectors/`)

**LocalAIDetector** (`LocalAIDetector.h/cpp`)
- ONNX Runtime integration
- Local AI model inference
- Text classification
- GPU/CPU backend support

```cpp
class LocalAIDetector : public TextDetector {
    // Uses ONNX model for local inference
    TextModerationResult analyzeText(const std::string& text) override;
    
private:
    Ort::Session* session_;
    Ort::Env env_;
    std::vector<float> tokenize(const std::string& text);
    float runInference(const std::vector<float>& tokens);
};
```

**HiveTextModerator** (`HiveTextModerator.h/cpp`)
- Hive API integration for text
- Multi-category classification
- Toxicity detection
- Language analysis

**HiveImageModerator** (`HiveImageModerator.h/cpp`)
- Hive Visual Moderation API v3
- Base64 image encoding
- 120+ classification categories
- NSFW, violence, hate symbol detection

```cpp
class HiveImageModerator : public ImageModerator {
    VisualModerationResult analyzeImage(
        const std::vector<uint8_t>& imageBytes,
        const std::string& mimeType
    ) override;
    
private:
    std::string base64Encode(const std::vector<uint8_t>& data);
    nlohmann::json buildRequest(const std::vector<uint8_t>& imageBytes);
};
```

#### Scraper Services (`include/scraper/`, `src/scraper/`)

**RedditScraper** (`RedditScraper.h/cpp`)
- Reddit API integration
- Subreddit content fetching
- Rate-limited requests
- JSON response parsing
- Pagination support

### 2.4 Data Layer

#### Storage Services (`include/storage/`, `src/storage/`)

**Storage Interface** (`Storage.h`)
- Abstract storage interface
- CRUD operations
- Query interface

**JsonlStorage** (`JsonlStorage.h/cpp`)
- JSONL (JSON Lines) format
- Sequential file writing
- Efficient append operations
- Line-by-line reading

#### Export/Import Services (`include/export/`, `src/export/`, `src/ui/MainWindow.cpp`)

**Exporter** (`Exporter.h/cpp`)
- Multiple format support (CSV, JSON, JSONL)
- Filtered exports
- Batch processing
- Progress reporting

**Import** (`MainWindow::onImportData`)
- CSV import with automatic field mapping
- JSON array import
- Option to clear or append to existing data
- Validation and error handling

---

## 3. Component Structure

### 3.1 Directory Structure

```
ModAI_cpp/
├── include/              # Header files
│   ├── core/            # Core business logic
│   │   ├── ContentItem.h
│   │   ├── ModerationEngine.h
│   │   └── RuleEngine.h
│   ├── detectors/       # AI detection modules
│   │   ├── HiveImageModerator.h
│   │   ├── HiveTextModerator.h
│   │   ├── LocalAIDetector.h
│   │   ├── ImageModerator.h
│   │   └── TextDetector.h
│   ├── export/          # Export functionality
│   │   └── Exporter.h
│   ├── network/         # Network layer
│   │   ├── HttpClient.h
│   │   ├── QtHttpClient.h
│   │   └── RateLimiter.h
│   ├── scraper/         # Content scrapers
│   │   └── RedditScraper.h
│   ├── storage/         # Data persistence
│   │   ├── Storage.h
│   │   └── JsonlStorage.h
│   ├── ui/              # User interface
│   │   ├── MainWindow.h
│   │   ├── ChatbotPanel.h
│   │   ├── AITextDetectorPanel.h
│   │   ├── AIImageDetectorPanel.h
│   │   ├── DetailPanel.h
│   │   ├── ReviewDialog.h
│   │   ├── DashboardModel.h
│   │   └── RailguardOverlay.h
│   └── utils/           # Utility functions
│       ├── Logger.h
│       ├── ImageUtils.h
│       └── WatermarkUtils.h
├── src/                 # Implementation files
│   ├── main.cpp        # Application entry point
│   ├── core/
│   ├── detectors/
│   ├── export/
│   ├── network/
│   ├── scraper/
│   ├── storage/
│   ├── ui/
│   └── utils/
├── config/             # Configuration files
│   └── rules.json     # Moderation rules
├── build/             # Build artifacts
├── data/              # Runtime data
└── docs/              # Documentation
```

### 3.2 Module Dependencies

```
┌─────────────┐
│     UI      │
│  (Qt6)      │
└──────┬──────┘
       │
       ├──────────┬──────────┬──────────┐
       ↓          ↓          ↓          ↓
┌──────────┐ ┌────────┐ ┌─────────┐ ┌────────┐
│Moderation│ │Scraper │ │ Export  │ │ Utils  │
│  Engine  │ │        │ │         │ │        │
└────┬─────┘ └───┬────┘ └────┬────┘ └────────┘
     │           │           │
     ├───────────┴───────────┤
     ↓                       ↓
┌─────────┐           ┌──────────┐
│Detectors│           │ Storage  │
│         │           │          │
└────┬────┘           └──────────┘
     │
     ├──────────┐
     ↓          ↓
┌─────────┐ ┌──────┐
│  Hive   │ │Local │
│   API   │ │  AI  │
└────┬────┘ └───┬──┘
     │          │
     └──────┬───┘
            ↓
    ┌──────────────┐
    │   Network    │
    │  (Qt/HTTP)   │
    └──────────────┘
```

---

## 4. Data Flow Architecture

### 4.1 Level 0 DFD (Context Diagram)

```
                    ┌─────────────┐
                    │   Hive AI   │
                    │  Services   │
                    └──────┬──────┘
                           │
                           ↓
┌──────────┐        ┌─────────────┐        ┌──────────┐
│   User   │───────→│   ModAI     │───────→│  Reddit  │
│          │←───────│  System     │←───────│   API    │
└──────────┘        └─────────────┘        └──────────┘
                           ↕
                    ┌─────────────┐
                    │ Local Files │
                    │   Storage   │
                    └─────────────┘
```

### 4.2 Level 1 DFD (System Decomposition)

```
┌─────────┐
│  User   │
└────┬────┘
     │
     │ Content Input
     ↓
┌────────────────────────┐
│   User Interface       │
│   (MainWindow)         │
└───────┬────────────────┘
        │
        ├─── Text ──────→ ┌──────────────────┐
        │                 │ Text Moderation  │
        │                 │    Subsystem     │
        │                 └────────┬─────────┘
        │                          │
        │                          ↓
        │                 ┌────────────────────┐
        │                 │  1. Local AI       │
        │                 │     Detector       │
        │                 ├────────────────────┤
        │                 │  2. Hive Text API  │
        │                 ├────────────────────┤
        │                 │  3. Rule Engine    │
        │                 └────────┬───────────┘
        │                          │
        ├─── Image ──────→ ┌──────┴───────────┐
        │                  │ Image Moderation │
        │                  │    Subsystem     │
        │                  └────────┬─────────┘
        │                           │
        │                           ↓
        │                  ┌───────────────────┐
        │                  │ Hive Visual API   │
        │                  │  - NSFW Detection │
        │                  │  - Violence Check │
        │                  │  - Hate Symbols   │
        │                  └────────┬──────────┘
        │                           │
        ├─── Scrape ─────→ ┌───────┴──────────┐
        │                  │ Reddit Scraper   │
        │                  │   Subsystem      │
        │                  └────────┬─────────┘
        │                           │
        │                           ↓
        └──────────────────→ ┌────────────────┐
                             │  Moderation    │
                             │    Engine      │
                             └────────┬───────┘
                                      │
                                      ↓
                             ┌────────────────┐
                             │  Result Cache  │
                             └────────┬───────┘
                                      │
                                      ↓
                             ┌────────────────┐
                             │    Storage     │
                             │   (JSONL)      │
                             └────────┬───────┘
                                      │
                                      ↓
                             ┌────────────────┐
                             │    Export      │
                             └────────────────┘
                                      │
                                      ↓ Files
                             ┌────────────────┐
                             │   User         │
                             └────────────────┘
```

### 4.3 Text Moderation Flow

```
User Input Text
      ↓
┌──────────────────┐
│  Input Validator │
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│  Result Cache    │ ← Check if analyzed before
│  (Lookup)        │
└────────┬─────────┘
         │
         ├─── Cache Hit ────→ Return Cached Result
         │
         └─── Cache Miss
                ↓
        ┌──────────────┐
        │ Rule Engine  │ → Quick reject based on rules
        └──────┬───────┘
               │
               ↓
        ┌──────────────────┐
        │  Local AI        │
        │  Detector        │ → ONNX inference
        │  (Primary)       │
        └──────┬───────────┘
               │
               ↓
        ┌──────────────────┐
        │  Hive Text API   │
        │  (Secondary)     │ → Multi-category analysis
        └──────┬───────────┘
               │
               ↓
        ┌──────────────────┐
        │  Score           │
        │  Aggregation     │ → Combine results
        └──────┬───────────┘
               │
               ↓
        ┌──────────────────┐
        │  Cache Storage   │ → Store for future
        └──────┬───────────┘
               │
               ↓
        ┌──────────────────┐
        │  UI Update       │ → Display results
        └──────────────────┘
```

### 4.4 Image Moderation Flow (Chatbot)

```
User Requests Image Generation
      ↓
┌──────────────────┐
│  Nebius API      │ → Generate image (flux-dev/schnell)
│  Image Gen       │
└────────┬─────────┘
         │
         ↓ Image URL
┌──────────────────┐
│  Download Image  │ → QNetworkAccessManager
└────────┬─────────┘
         │
         ↓ Image Bytes (WebP/PNG/JPEG)
┌──────────────────┐
│  Base64 Encode   │ → Prepare for API
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│  Hive Visual API │
│  (JSON Request)  │ → { "input": [{ "media_base64": "..." }] }
└────────┬─────────┘
         │
         ↓ Classification Results
┌──────────────────┐
│  Category Filter │ → Filter "no_" labels
│  (120+ classes)  │ → Check harmful categories
└────────┬─────────┘
         │
         ├─── Blocked (Score > 50%)
         │        ↓
         │   ┌──────────────────┐
         │   │  Display Block   │
         │   │  Warning         │
         │   └────────┬─────────┘
         │            │
         │            ↓
         │   ┌──────────────────┐
         │   │  "Show Anyway"   │ ← User override
         │   │  Button          │
         │   └────────┬─────────┘
         │            │
         │            ↓
         │   ┌──────────────────┐
         │   │  Display Blurred │
         │   │  Keep Red Bubble │
         │   └────────┬─────────┘
         │            │
         │            ↓
         │   ┌──────────────────┐
         │   │  "Hide Image"    │ ← Restore blocked state
         │   │  Button          │
         │   └──────────────────┘
         │
         └─── Passed (Score < 50%)
                  ↓
         ┌──────────────────┐
         │  Display Image   │
         │  with Watermark  │
         └────────┬─────────┘
                  │
                  ↓
         ┌──────────────────┐
         │  Download Button │ → Save to disk
         └──────────────────┘
```

### 4.5 Reddit Scraping Flow

```
User Initiates Scrape
      ↓
┌──────────────────┐
│  Configuration   │ → Subreddit, limit, timeframe
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│  Rate Limiter    │ → Check API limits
└────────┬─────────┘
         │
         ↓
┌──────────────────┐
│  Reddit API      │ → GET /r/{subreddit}/hot.json
└────────┬─────────┘
         │
         ↓ JSON Response
┌──────────────────┐
│  Parser          │ → Extract posts, comments
└────────┬─────────┘
         │
         ↓ Post Objects
┌──────────────────┐
│  Moderation Loop │
│  (Each Post)     │
└────────┬─────────┘
         │
         ├─── Text Content ──→ Text Moderation Flow
         │
         └─── Image URLs ────→ Image Moderation Flow
                  ↓
         ┌──────────────────┐
         │  Result          │
         │  Aggregation     │
         └────────┬─────────┘
                  │
                  ↓
         ┌──────────────────┐
         │  Storage         │ → Save to JSONL
         └────────┬─────────┘
                  │
                  ↓
         ┌──────────────────┐
         │  Dashboard       │ → Display in table
         │  Update          │
         └──────────────────┘
```

---

## 5. Class Hierarchy

### 5.1 Core Classes

```
ContentItem (struct)
  - Properties: id, content, type, status, aiScore, flags, timestamp
  - Used by: ModerationEngine, Storage, UI components

ModerationEngine (class)
  ├── Dependencies:
  │   ├── TextDetector* (interface)
  │   ├── ImageModerator* (interface)
  │   └── RuleEngine
  ├── Methods:
  │   ├── moderateText(string) → ModerationResult
  │   ├── moderateImage(vector<uint8_t>) → ModerationResult
  │   ├── moderateBatch(vector<ContentItem>) → vector<ModerationResult>
  │   └── updateRules()
  └── Used by: UI Panels, RedditScraper

RuleEngine (class)
  ├── Data: vector<Rule> rules_
  ├── Methods:
  │   ├── loadRules(string jsonPath)
  │   ├── evaluateContent(ContentItem) → bool
  │   └── getMatchedRules(ContentItem) → vector<string>
  └── Rule Structure:
      ├── pattern (regex)
      ├── action (BLOCK/FLAG/ALLOW)
      └── priority (int)

### 5.2 Detector Classes

```
TextDetector (interface)
  │
  ├── LocalAIDetector (implementation)
  │   ├── ONNX Runtime session
  │   ├── Tokenizer
  │   ├── Methods:
  │   │   ├── analyzeText(string) → TextModerationResult
  │   │   ├── tokenize(string) → vector<float>
  │   │   └── runInference(vector<float>) → float
  │   └── Model: ai_detector.onnx
  │
  └── HiveTextModerator (implementation)
      ├── HTTP Client
      ├── Rate Limiter
      ├── Methods:
      │   ├── analyzeText(string) → TextModerationResult
      │   └── parseResponse(json) → TextModerationResult
      └── Categories:
          ├── toxicity
          ├── severe_toxicity
          ├── identity_attack
          ├── insult
          ├── threat
          └── profanity

ImageModerator (interface)
  │
  └── HiveImageModerator (implementation)
      ├── HTTP Client
      ├── Rate Limiter
      ├── Methods:
      │   ├── analyzeImage(vector<uint8_t>, string mime) → VisualModerationResult
      │   ├── base64Encode(vector<uint8_t>) → string
      │   └── parseResponse(json) → VisualModerationResult
      └── Categories (120+):
          ├── NSFW: general_nsfw, yes_female_nudity, yes_male_nudity
          ├── Suggestive: general_suggestive, yes_cleavage, yes_underwear
          ├── Violence: gun_in_hand, very_bloody, yes_fight
          ├── Drugs: yes_pills, yes_smoking, illicit_injectables
          └── Hate: yes_nazi, yes_kkk, yes_terrorist
```

### 5.3 Network Classes

```
HttpClient (interface)
  │
  └── QtHttpClient (implementation)
      ├── QNetworkAccessManager
      ├── Methods:
      │   ├── get(HttpRequest) → HttpResponse
      │   ├── post(HttpRequest) → HttpResponse
      │   ├── put(HttpRequest) → HttpResponse
      │   ├── del(HttpRequest) → HttpResponse
      │   └── executeRequest(QNetworkReply*) → HttpResponse
      ├── Features:
      │   ├── Multipart form-data
      │   ├── JSON body encoding
      │   ├── Base64 encoding
      │   ├── Retry logic
      │   ├── Timeout handling
      │   └── Thread-safe networking
      └── Used by: All API integrations

RateLimiter (class)
  ├── Algorithm: Token Bucket
  ├── Properties:
  │   ├── maxRequests (int)
  │   ├── timeWindow (duration)
  │   └── tokens (atomic<int>)
  ├── Methods:
  │   ├── waitIfNeeded() → void
  │   ├── tryAcquire() → bool
  │   └── reset()
  └── Used by: HiveTextModerator, HiveImageModerator, RedditScraper
```

### 5.4 Storage Classes

```
Storage (interface)
  │
  └── JsonlStorage (implementation)
      ├── File: std::ofstream
      ├── Methods:
      │   ├── save(ContentItem)
      │   ├── saveBatch(vector<ContentItem>)
      │   ├── load() → vector<ContentItem>
      │   ├── loadFiltered(predicate) → vector<ContentItem>
      │   └── clear()
      └── Format: JSON Lines (one JSON object per line)

Exporter (class)
  ├── Formats: CSV, JSON, JSONL
  ├── Methods:
  │   ├── exportToCSV(vector<ContentItem>, string path)
  │   ├── exportToJSON(vector<ContentItem>, string path)
  │   ├── exportToJSONL(vector<ContentItem>, string path)
  │   └── exportFiltered(predicate, Format, string path)
  └── Used by: MainWindow (Export menu)
```

### 5.5 UI Classes

```
QMainWindow
  │
  └── MainWindow (class)
      ├── QTabWidget* tabWidget_
      ├── Panels:
      │   ├── ChatbotPanel*
      │   ├── AITextDetectorPanel*
      │   ├── AIImageDetectorPanel*
      │   └── Dashboard Tab (QTableView)
      ├── Menu System:
      │   ├── File: Export, Exit
      │   ├── View: Refresh, Filters
      │   └── Help: About
      └── Status Bar

QWidget
  │
  ├── ChatbotPanel (class)
  │   ├── UI Elements:
  │   │   ├── QTextEdit* chatDisplay_
  │   │   ├── QLineEdit* userInput_
  │   │   ├── QPushButton* sendButton_
  │   │   ├── QComboBox* modelSelector_
  │   │   └── QProgressBar* generationProgress_
  │   ├── Dependencies:
  │   │   ├── ImageModerator*
  │   │   ├── QNetworkAccessManager*
  │   │   └── WatermarkUtils
  │   ├── Methods:
  │   │   ├── sendMessage()
  │   │   ├── handleChatResponse(string)
  │   │   ├── handleImageGeneration(string prompt)
  │   │   ├── moderateImage(QByteArray, QString mime) → bool
  │   │   └── displayBlockedImage(bool show)
  │   └── Features:
  │       ├── Real-time chat
  │       ├── Image generation
  │       ├── Inline moderation
  │       ├── Show/Hide blocked content
  │       └── Download generated images
  │
  ├── AITextDetectorPanel (class)
  │   ├── UI Elements:
  │   │   ├── QTextEdit* inputText_
  │   │   ├── QLabel* resultLabel_
  │   │   ├── QProgressBar* confidenceBar_
  │   │   └── QPushButton* analyzeButton_
  │   ├── Dependencies:
  │   │   └── LocalAIDetector*
  │   └── Methods:
  │       ├── analyzeText()
  │       └── displayResults(TextModerationResult)
  │
  └── AIImageDetectorPanel (class)
      ├── UI Elements:
      │   ├── QLabel* imagePreview_
      │   ├── QPushButton* uploadButton_
      │   ├── QTableWidget* resultsTable_
      │   └── QProgressBar* analysisProgress_
      ├── Dependencies:
      │   └── HiveImageModerator*
      └── Methods:
          ├── uploadImage()
          ├── analyzeImage(QString path)
          └── displayResults(VisualModerationResult)

QAbstractTableModel
  │
  └── DashboardModel (class)
      ├── Data: vector<ContentItem> items_
      ├── Methods:
      │   ├── rowCount() → int
      │   ├── columnCount() → int
      │   ├── data(QModelIndex, int role) → QVariant
      │   ├── headerData() → QVariant
      │   ├── addItem(ContentItem)
      │   ├── removeItem(int row)
      │   └── updateItem(int row, ContentItem)
      └── Columns:
          ├── ID
          ├── Type
          ├── Content (truncated)
          ├── AI Score
          ├── Status
          └── Timestamp

QSortFilterProxyModel
  │
  └── DashboardProxyModel (class)
      ├── Methods:
      │   ├── filterAcceptsRow() → bool
      │   ├── lessThan() → bool
      │   ├── setStatusFilter(ModerationStatus)
      │   └── setScoreThreshold(double)
      └── Features:
          ├── Multi-column sorting
          ├── Status filtering
          ├── Score range filtering
          └── Text search
```

---

## 6. Module Interactions

### 6.1 Initialization Sequence

```
main()
  │
  ├──→ QApplication app
  │
  ├──→ Initialize Logger
  │     │
  │     └──→ Setup file logging (data/logs/)
  │
  ├──→ Load Configuration
  │     │
  │     ├──→ API Keys (environment variables)
  │     └──→ Rules (config/rules.json)
  │
  ├──→ Create Core Components
  │     │
  │     ├──→ QtHttpClient
  │     ├──→ LocalAIDetector (ONNX model)
  │     ├──→ HiveTextModerator
  │     ├──→ HiveImageModerator
  │     ├──→ RuleEngine
  │     └──→ JsonlStorage
  │
  ├──→ Create ModerationEngine
  │     │
  │     ├──→ Inject TextDetector
  │     ├──→ Inject ImageModerator
  │     └──→ Inject RuleEngine
  │
  ├──→ Create MainWindow
  │     │
  │     ├──→ Create ChatbotPanel
  │     │     └──→ Inject ImageModerator
  │     │
  │     ├──→ Create AITextDetectorPanel
  │     │     └──→ Inject LocalAIDetector
  │     │
  │     ├──→ Create AIImageDetectorPanel
  │     │     └──→ Inject HiveImageModerator
  │     │
  │     └──→ Create Dashboard Tab
  │           └──→ Create DashboardModel + ProxyModel
  │
  ├──→ Show MainWindow
  │
  └──→ app.exec()
```

### 6.2 Text Moderation Interaction

```
User Enters Text in AITextDetectorPanel
  │
  └──→ analyzeButton clicked()
        │
        └──→ AITextDetectorPanel::analyzeText()
              │
              ├──→ Show progress indicator
              │
              ├──→ LocalAIDetector::analyzeText(text)
              │     │
              │     ├──→ tokenize(text) → vector<float>
              │     │
              │     ├──→ ONNX Runtime inference
              │     │
              │     └──→ return TextModerationResult
              │
              ├──→ Update UI with results
              │     │
              │     ├──→ Set result label
              │     ├──→ Update confidence bar
              │     └──→ Apply color coding
              │
              └──→ Hide progress indicator
```

### 6.3 Image Generation & Moderation Interaction

```
User Requests Image in ChatbotPanel
  │
  └──→ Sends message with /imagine or prompt
        │
        └──→ ChatbotPanel::handleImageGeneration(prompt)
              │
              ├──→ Show typing indicator
              │
              ├──→ Call Nebius Image API
              │     │
              │     ├──→ POST /v1/image/generations
              │     │     Body: { model, prompt, size }
              │     │
              │     └──→ Returns: { url: "https://..." }
              │
              ├──→ Download image
              │     │
              │     └──→ QNetworkAccessManager::get(url)
              │
              ├──→ ChatbotPanel::moderateImage(imageData, mime)
              │     │
              │     ├──→ HiveImageModerator::analyzeImage()
              │     │     │
              │     │     ├──→ base64Encode(imageData)
              │     │     │
              │     │     ├──→ Build JSON request
              │     │     │     { "input": [{ "media_base64": "..." }] }
              │     │     │
              │     │     ├──→ POST to Hive API
              │     │     │
              │     │     ├──→ Parse response
              │     │     │     { "output": [{ "classes": [...] }] }
              │     │     │
              │     │     └──→ Filter categories (ignore "no_" labels)
              │     │
              │     ├──→ Evaluate against thresholds
              │     │     │
              │     │     └──→ Check harmful categories > 50%
              │     │
              │     └──→ return bool (passed/blocked)
              │
              ├──→ If blocked:
              │     │
              │     ├──→ Display warning message
              │     ├──→ Set red bubble style
              │     ├──→ Add "Show Anyway" button
              │     │     │
              │     │     └──→ onClick: Display blurred image, keep red style
              │     │           Add "Hide Image" button
              │     │
              │     └──→ Log: "Image blocked"
              │
              └──→ If passed:
                    │
                    ├──→ Add watermark
                    ├──→ Display image in chat
                    ├──→ Add download button
                    └──→ Log: "Image passed moderation"
```

### 6.4 Reddit Scraping Interaction

```
User Clicks "Scrape" in Dashboard
  │
  └──→ MainWindow::scrapeReddit()
        │
        ├──→ Show configuration dialog
        │     │
        │     └──→ User inputs: subreddit, limit, timeframe
        │
        └──→ RedditScraper::scrape(config)
              │
              ├──→ RateLimiter::waitIfNeeded()
              │
              ├──→ Build API URL
              │     │
              │     └──→ https://www.reddit.com/r/{subreddit}/{sort}.json
              │
              ├──→ HttpClient::get(request)
              │
              ├──→ Parse JSON response
              │     │
              │     └──→ Extract posts from data.children
              │
              └──→ For each post:
                    │
                    ├──→ Create ContentItem
                    │     │
                    │     ├──→ id = post.data.id
                    │     ├──→ content = post.data.title + selftext
                    │     ├──→ type = TEXT or IMAGE
                    │     └──→ status = PENDING
                    │
                    ├──→ ModerationEngine::moderateContent(item)
                    │     │
                    │     ├──→ If text: LocalAIDetector + HiveTextModerator
                    │     │
                    │     ├──→ If image: HiveImageModerator
                    │     │
                    │     └──→ Aggregate results
                    │
                    ├──→ Update ContentItem with results
                    │
                    ├──→ JsonlStorage::save(item)
                    │
                    └──→ DashboardModel::addItem(item)
                          │
                          └──→ Triggers table refresh
```

---

## 7. External Integrations

### 7.1 Hive AI API

**Text Moderation API**
- **Endpoint:** `https://api.thehive.ai/api/v2/task/sync`
- **Method:** POST
- **Authentication:** Bearer Token (API Key)
- **Request Format:**
  ```json
  {
    "text_data": [
      {
        "text": "Content to moderate"
      }
    ]
  }
  ```
- **Response Format:**
  ```json
  {
    "status": [
      {
        "response": {
          "output": [
            {
              "toxicity": { "score": 0.95 },
              "severe_toxicity": { "score": 0.12 }
            }
          ]
        }
      }
    ]
  }
  ```

**Visual Moderation API v3**
- **Endpoint:** `https://api.thehive.ai/api/v3/hive/visual-moderation`
- **Method:** POST
- **Authentication:** Bearer Token
- **Request Format:**
  ```json
  {
    "input": [
      {
        "media_base64": "base64_encoded_image_data"
      }
    ]
  }
  ```
- **Response Format:**
  ```json
  {
    "task_id": "...",
    "model": "hive/visual-moderation",
    "output": [
      {
        "classes": [
          { "class_name": "general_nsfw", "value": 0.99 },
          { "class_name": "yes_female_nudity", "value": 0.95 }
        ]
      }
    ]
  }
  ```
- **Supported Formats:** JPG, PNG, WebP, GIF (images); MP4, WebM (videos)
- **Size Limits:**
  - Base64: 20MB
  - Multipart: 200MB
- **Rate Limits:** 100 requests/day (Playground)

### 7.2 Nebius Image Generation API

- **Endpoint:** `https://api.nebius.ai/v1/image/generations`
- **Method:** POST
- **Authentication:** Bearer Token
- **Models:**
  - `black-forest-labs/flux-dev` (higher quality, slower)
  - `black-forest-labs/flux-schnell` (faster, lower quality)
- **Request Format:**
  ```json
  {
    "model": "black-forest-labs/flux-dev",
    "prompt": "A beautiful landscape",
    "n": 1,
    "size": "1024x1024"
  }
  ```
- **Response Format:**
  ```json
  {
    "created": 1702834567,
    "data": [
      {
        "url": "https://pictures-storage.storage.eu-north1.nebius.cloud/..."
      }
    ]
  }
  ```

### 7.3 Reddit API

- **Endpoint:** `https://www.reddit.com/r/{subreddit}/{sort}.json`
- **Method:** GET
- **Authentication:** None (public endpoints)
- **Parameters:**
  - `limit`: Number of posts (max 100)
  - `after`: Pagination token
  - `t`: Time filter (hour, day, week, month, year, all)
- **Response Format:**
  ```json
  {
    "data": {
      "children": [
        {
          "data": {
            "id": "abc123",
            "title": "Post title",
            "selftext": "Post content",
            "url": "https://...",
            "score": 42,
            "num_comments": 10
          }
        }
      ]
    }
  }
  ```
- **Rate Limits:** 60 requests/minute

### 7.4 ONNX Runtime (Local AI)

- **Library:** ONNX Runtime C++ API
- **Model:** `ai_detector.onnx` (custom trained)
- **Input:** Tokenized text (float vector)
- **Output:** AI probability score (0.0 - 1.0)
- **Backend:** CPU or GPU (configurable)
- **Model Location:** `~/.local/share/ModAI/ModAI/data/models/`

---

## 8. Storage Architecture

### 8.1 File System Structure

```
~/.local/share/ModAI/ModAI/
├── data/
│   ├── models/
│   │   └── ai_detector.onnx          # Local AI model
│   ├── logs/
│   │   └── modai_YYYY-MM-DD.log      # Daily logs
│   └── storage/
│       └── moderated_content.jsonl   # Persisted results
├── generated_images/
│   └── YYYYMMDD_HHMMSS.png          # Downloaded images
└── exports/
    ├── export_YYYYMMDD_HHMMSS.csv
    ├── export_YYYYMMDD_HHMMSS.json
    └── export_YYYYMMDD_HHMMSS.jsonl
```

### 8.2 JSONL Storage Format

Each line is a complete JSON object:

```json
{"id":"abc123","type":"TEXT","content":"Sample text","status":"APPROVED","aiScore":0.15,"flags":[],"timestamp":"2025-12-18T02:30:45Z"}
{"id":"def456","type":"IMAGE","content":"https://...","status":"REJECTED","aiScore":0.95,"flags":["general_nsfw","yes_nudity"],"timestamp":"2025-12-18T02:31:12Z"}
```

### 8.4 Configuration Files

**config/rules.json**
```json
{
  "rules": [
    {
      "id": "explicit_language",
      "pattern": "\\b(badword1|badword2)\\b",
      "action": "BLOCK",
      "priority": 1,
      "description": "Block explicit language"
    },
    {
      "id": "spam_pattern",
      "pattern": "buy now|click here|limited time",
      "action": "FLAG",
      "priority": 2
    }
  ]
}
```

---

## 9. Security Model

### 9.1 API Key Management

- **Storage:** Environment variables (not hardcoded)
- **Access:** Read once at startup
- **Transmission:** HTTPS only
- **Headers:** Bearer token authentication

```cpp
// Environment variables
HIVE_API_KEY=your_hive_key_here
NEBIUS_API_KEY=your_nebius_key_here
REDDIT_CLIENT_ID=your_reddit_id
```

### 9.2 Data Privacy

- **Local Processing:** Text detection via ONNX (no data sent externally)
- **Logs:** Local filesystem only
- **Generated Images:** Watermarked with metadata
- **User Content:** Never logged or persisted without consent

### 9.3 Network Security

- **Protocol:** HTTPS/TLS 1.2+
- **Certificate Validation:** Enabled
- **Timeout:** 60 seconds per request
- **Retry Logic:** Max 3 retries for 5xx errors
- **Rate Limiting:** Per-API token bucket

---

## 10. Deployment Architecture

### 10.1 Build Process

```bash
# Dependencies
- Qt 6.5+
- CMake 3.20+
- C++17 compiler (GCC/Clang/MSVC)
- ONNX Runtime 1.16+
- nlohmann/json

# Build steps
mkdir build && cd build
cmake ..
make -j4
```

### 10.2 Runtime Dependencies

- **Qt Libraries:** Core, Gui, Widgets, Network
- **ONNX Runtime:** libonnxruntime.so (Linux) / onnxruntime.dll (Windows)
- **System Libraries:** pthread, dl, m

### 10.3 Deployment Targets

- **Linux:** Ubuntu 20.04+, Fedora 35+
- **Windows:** Windows 10/11
- **macOS:** 11.0+ (Big Sur and later)

### 10.4 Performance Considerations

**Memory Usage:**
- Base application: ~100MB
- ONNX model loaded: +200MB
- Qt UI components: ~50MB
- Network buffers: ~20MB per active request
- Image caching: Variable (depends on generated images)

**CPU Usage:**
- Idle: < 1%
- Text inference: 10-20% per analysis
- Image moderation: 5-10% (mostly network I/O)
- UI rendering: 2-5%

**Network Usage:**
- Text moderation: ~1KB per request
- Image moderation: ~100KB-2MB per image (base64 encoded)
- Image generation: ~500KB-2MB per generated image
- Reddit scraping: ~50KB per 100 posts

---

## Appendix A: Key Algorithms

### A.1 Token Bucket Rate Limiter

```cpp
class RateLimiter {
private:
    std::atomic<int> tokens_;
    const int maxTokens_;
    const std::chrono::milliseconds refillInterval_;
    std::chrono::steady_clock::time_point lastRefill_;
    std::mutex mutex_;
    
public:
    void waitIfNeeded() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Refill tokens based on time elapsed
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - lastRefill_;
        int tokensToAdd = elapsed / refillInterval_;
        
        if (tokensToAdd > 0) {
            tokens_ = std::min(maxTokens_, tokens_ + tokensToAdd);
            lastRefill_ = now;
        }
        
        // Wait if no tokens available
        while (tokens_ <= 0) {
            lock.unlock();
            std::this_thread::sleep_for(refillInterval_);
            lock.lock();
            // Refill after sleep
            tokens_++;
        }
        
        tokens_--;
    }
};
```

### A.2 Content Hash Generation

```cpp
std::string hashContent(const std::string& content) {
    // Simple FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : content) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return std::to_string(hash);
}
```

### A.3 Score Aggregation

```cpp
double aggregateScores(const std::vector<double>& scores) {
    if (scores.empty()) return 0.0;
    
    // Weighted average with higher weight to max score
    double maxScore = *std::max_element(scores.begin(), scores.end());
    double avgScore = std::accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
    
    // 70% max, 30% average
    return 0.7 * maxScore + 0.3 * avgScore;
}
```

---

## Appendix B: API Error Handling

### B.1 HTTP Status Code Handling

| Status Code | Action | Retry |
|------------|--------|-------|
| 200 OK | Process response | No |
| 400 Bad Request | Log error, return empty result | No |
| 401 Unauthorized | Log error, check API key | No |
| 403 Forbidden | Log error, API key invalid | No |
| 429 Too Many Requests | Wait and retry | Yes (3x) |
| 500 Internal Server Error | Wait and retry | Yes (3x) |
| 502/503/504 Gateway errors | Wait and retry | Yes (3x) |
| Network timeout | Retry with increased timeout | Yes (2x) |

### B.2 Retry Strategy

```cpp
int retryDelayMs = 1000;  // Start with 1 second
for (int attempt = 0; attempt <= maxRetries; attempt++) {
    HttpResponse response = httpClient->post(request);
    
    if (response.success) {
        return response;
    }
    
    if (shouldRetry(response.statusCode)) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(retryDelayMs * (1 << attempt))
        );  // Exponential backoff
    } else {
        break;  // Don't retry 4xx errors
    }
}
```

---

## Appendix C: Configuration Options

### C.1 Environment Variables

```bash
# Required
HIVE_API_KEY=<your_hive_api_key>
NEBIUS_API_KEY=<your_nebius_api_key>

# Optional
MODAI_LOG_LEVEL=DEBUG|INFO|WARN|ERROR  # Default: INFO
MODAI_MAX_RETRIES=3                     # Max HTTP retries
MODAI_TIMEOUT_MS=60000                  # Request timeout
ONNX_THREADS=4                          # ONNX inference threads
```

### C.2 Runtime Configuration

Located in `~/.config/ModAI/config.json`:

```json
{
  "moderation": {
    "textThreshold": 0.7,
    "imageThreshold": 0.5,
    "useLocalAI": true,
    "useHiveAPI": true
  },
  "ui": {
    "theme": "light",
    "refreshInterval": 5000,
    "maxHistoryItems": 1000
  }
}
```

---

## Document Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-18 | System | Initial comprehensive architecture document |

---

## Contact & Support

For technical questions or contributions:
- GitHub: [Repository URL]
- Documentation: `/docs/` directory
- Issue Tracker: [Issues URL]

---

*This document provides a complete architectural overview suitable for creating DFD diagrams, class diagrams, sequence diagrams, and other technical documentation.*
