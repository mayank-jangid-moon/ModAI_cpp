# ModAI Backend - Complete API Reference (Updated)

## Base URL
```
http://localhost:8080/api
```

## Table of Contents
1. [Health & Info](#health--info)
2. [Content Management](#content-management)
3. [Content Moderation](#content-moderation)
4. [Reddit Scraper](#reddit-scraper)
5. [Chatbot with Railguard](#chatbot-with-railguard)
6. [AI Detection](#ai-detection)
7. [Statistics](#statistics)
8. [Data Export](#data-export)

---

## Health & Info

### Health Check
```http
GET /api/health
```

**Response:**
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "timestamp": 1734393600
}
```

---

## Content Management

### Get All Content
```http
GET /api/content?filter={decision}&subreddit={name}&type={text|image}
```

**Query Parameters:**
- `filter` (optional): `allow`, `block`, or `review`
- `subreddit` (optional): Filter by subreddit name
- `type` (optional): `text` or `image`

**Response:**
```json
[
  {
    "id": "abc123",
    "content_type": "text",
    "text_content": "Sample content",
    "source": "reddit",
    "subreddit": "technology",
    "timestamp": 1734393600,
    "ai_detection": {
      "label": "human",
      "ai_score": 0.15,
      "confidence": 0.85,
      "model": "local_detector"
    },
    "moderation": {
      "provider": "hive",
      "labels": {
        "sexual": 0.01,
        "violence": 0.02,
        "hate": 0.0,
        "harassment": 0.0,
        "self_harm": 0.0,
        "illicit": 0.0
      }
    },
    "decision": {
      "auto_action": "allow",
      "auto_reason": "All scores below threshold",
      "rules_applied": ["default_rule"]
    },
    "metadata": {
      "author": "user123",
      "score": "45",
      "title": "Post Title"
    }
  }
]
```

### Get Single Content Item
```http
GET /api/content/{id}
```

**Response:** Single ContentItem object (same structure as above)

### Update Content Decision (Human Review)
```http
PUT /api/content/{id}/decision
```

**Request Body:**
```json
{
  "decision": "allow",  // or "block"
  "reviewer": "admin_user",
  "notes": "Reviewed manually, content is acceptable"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Decision updated",
  "content": { /* Updated ContentItem */ }
}
```

---

## Content Moderation

### Moderate Text
```http
POST /api/moderate/text
```

**Request Body:**
```json
{
  "text": "Text content to moderate",
  "source": "api",  // optional
  "metadata": {     // optional
    "custom_field": "value"
  }
}
```

**Response:** ContentItem object with moderation results

### Moderate Image
```http
POST /api/moderate/image
```

**Request:**
- Content-Type: `multipart/form-data`
- Field name: `image`
- Value: Image file

**Response:** ContentItem object with moderation results

---

## Reddit Scraper

### Get Scraper Status
```http
GET /api/reddit/status
```

**Response:**
```json
{
  "is_running": true,
  "subreddits": ["technology", "programming", "MachineLearning"]
}
```

### Start Scraping
```http
POST /api/reddit/start
```

**Request Body:**
```json
{
  "subreddits": ["technology", "programming"],
  "interval": 300  // seconds, default: 300 (5 minutes)
}
```

**Response:**
```json
{
  "success": true,
  "message": "Reddit scraper started",
  "subreddits": ["technology", "programming"],
  "interval_seconds": 300
}
```

**Notes:**
- Requires Reddit API credentials (see Environment Variables below)
- Scraped items are automatically moderated and stored
- Use `/api/content?source=reddit` to retrieve scraped items

### Stop Scraping
```http
POST /api/reddit/stop
```

**Response:**
```json
{
  "success": true,
  "message": "Reddit scraper stopped"
}
```

### Scrape Once (Manual Trigger)
```http
POST /api/reddit/scrape
```

**Request Body:**
```json
{
  "subreddits": ["technology"]  // optional, uses configured subreddits if omitted
}
```

**Response:**
```json
{
  "success": true,
  "items_scraped": 25,
  "items": [
    { /* ContentItem */ },
    { /* ContentItem */ }
  ]
}
```

### Get Reddit Items
```http
GET /api/reddit/items?subreddit={name}
```

**Query Parameters:**
- `subreddit` (optional): Filter by specific subreddit

**Response:** Array of ContentItem objects from Reddit

---

## Chatbot with Railguard

### Send Chat Message
```http
POST /api/chat
```

**Request Body:**
```json
{
  "message": "Hello, how are you?"
}
```

**Response (Normal):**
```json
{
  "id": "msg123",
  "message": "I'm a demo assistant with Railguard protection...",
  "blocked": false,
  "timestamp": 1734393600,
  "moderation_score": {
    "user_message": {
      "sexual": 0.0,
      "violence": 0.0,
      "hate": 0.0
    },
    "ai_response": {
      "sexual": 0.0,
      "violence": 0.0,
      "hate": 0.0
    }
  }
}
```

**Response (Blocked User Message):**
```json
{
  "id": "msg124",
  "blocked": true,
  "reason": "Message contains inappropriate content",
  "moderation_details": {
    "sexual": 0.92,
    "violence": 0.05,
    "hate": 0.15,
    "harassment": 0.23
  }
}
```

**How it works:**
1. User message is moderated FIRST
2. If blocked, returns immediately with block reason
3. If passed, generates AI response
4. AI response is moderated BEFORE sending
5. If AI response blocked, returns safe fallback message

### Get Chat History
```http
GET /api/chat/history?limit=50
```

**Query Parameters:**
- `limit` (optional): Max messages to return (default: 50)

**Response:**
```json
[
  {
    "id": "msg123",
    "role": "user",
    "content": "Hello",
    "timestamp": 1734393600,
    "was_blocked": false,
    "block_reason": ""
  },
  {
    "id": "msg124",
    "role": "assistant",
    "content": "Hi there!",
    "timestamp": 1734393601,
    "was_blocked": false,
    "block_reason": ""
  }
]
```

### Clear Chat History
```http
DELETE /api/chat/history
```

**Response:**
```json
{
  "success": true,
  "message": "Chat history cleared"
}
```

---

## AI Detection

### Detect AI-Generated Content & Identify Source
```http
POST /api/detect/ai
```

**Request Body:**
```json
{
  "text": "Text to analyze for AI generation"
}
```

**Response:**
```json
{
  "ai_score": 0.92,
  "ai_confidence": 0.88,
  "is_ai_generated": true,
  "detected_source": "ChatGPT (OpenAI)",
  "indicators": [
    "Contains ChatGPT-specific phrasing",
    "Well-structured with paragraphs",
    "Formal sentence structure"
  ],
  "model_used": "local_detector",
  "analysis": {
    "text_length": 450,
    "sentence_count": 8,
    "avg_word_length": 5.2
  }
}
```

**Detected Sources:**
- `"ChatGPT (OpenAI)"` - Identified OpenAI patterns
- `"Claude (Anthropic)"` - Identified Anthropic patterns
- `"Gemini (Google)"` - Identified Google patterns
- `"Generic LLM"` - High AI probability but no specific markers
- `"unknown"` - Low AI probability or inconclusive

**Indicators (Examples):**
- "Contains Claude-specific phrasing"
- "Contains ChatGPT-specific phrasing"
- "Contains Gemini-specific phrasing"
- "Well-structured with paragraphs"
- "Formal sentence structure"
- "High AI probability without specific markers"

---

## Statistics

### Get Comprehensive Statistics
```http
GET /api/stats
```

**Response:**
```json
{
  "total_items": 1523,
  "blocked": 45,
  "review": 12,
  "allowed": 1466,
  "text_items": 1200,
  "image_items": 323,
  "ai_generated": 234,
  "reddit_items": 856,
  "avg_ai_score": 0.23,
  "avg_sexual_score": 0.02,
  "avg_violence_score": 0.01,
  "subreddit_breakdown": {
    "technology": 345,
    "programming": 289,
    "MachineLearning": 222
  },
  "chat_messages": 67
}
```

---

## Data Export

### Export Data
```http
GET /api/export?format={csv|json}
```

**Query Parameters:**
- `format`: `csv` or `json` (default: `json`)

**Response (Plain Text):**
```
CSV export created: ./data/exports/1734393600.csv
```
or
```
JSON export created: ./data/exports/1734393600.json
```

**Note:** Files are saved on the server. To download, use the file path returned.

---

## Data Models

### ContentItem
```typescript
interface ContentItem {
  id: string;
  content_type: "text" | "image" | "both";
  text_content: string;
  image_path?: string;
  source: "reddit" | "api" | "chatbot";
  subreddit: string;
  timestamp: number;
  
  ai_detection: {
    label: "ai" | "human";
    ai_score: number;      // 0.0 to 1.0
    confidence: number;    // 0.0 to 1.0
    model: string;
  };
  
  moderation: {
    provider: "hive";
    labels: {
      sexual: number;      // 0.0 to 1.0
      violence: number;
      hate: number;
      harassment: number;
      self_harm: number;
      illicit: number;
    };
  };
  
  decision: {
    auto_action: "allow" | "block" | "review";
    auto_reason: string;
    rules_applied: string[];
    human_decision?: string;
    human_reviewer?: string;
    human_notes?: string;
    human_review_timestamp?: number;
  };
  
  metadata: {
    [key: string]: string;
  };
}
```

### ChatMessage
```typescript
interface ChatMessage {
  id: string;
  role: "user" | "assistant";
  content: string;
  timestamp: number;
  was_blocked: boolean;
  block_reason?: string;
}
```

---

## Environment Variables

Set these before starting the backend server:

```bash
# Hive AI API Key (required for content moderation)
export MODAI_HIVE_API_KEY="your_hive_api_key_here"

# Reddit API Credentials (required for Reddit scraper)
export MODAI_REDDIT_CLIENT_ID="your_reddit_client_id"
export MODAI_REDDIT_CLIENT_SECRET="your_reddit_client_secret"
```

**Getting API Keys:**
- **Hive AI**: Sign up at https://thehive.ai/
- **Reddit**: Create app at https://www.reddit.com/prefs/apps

---

## Server Commands

### Start Server
```bash
cd backend/build

# Basic server
./ModAI_Backend --port 8080

# With Reddit scraper
./ModAI_Backend --port 8080 --enable-reddit

# With AI detection model
./ModAI_Backend --model ../models/detector.onnx --tokenizer ../models/tokenizer.json

# Full configuration
./ModAI_Backend \
  --port 8080 \
  --data ../data \
  --rules ../config/rules.json \
  --model ../models/detector.onnx \
  --tokenizer ../models/tokenizer.json \
  --enable-reddit
```

### Command-Line Options
- `--port <num>`: Server port (default: 8080)
- `--data <path>`: Data directory (default: ./data)
- `--rules <path>`: Rules JSON file (default: ./config/rules.json)
- `--model <path>`: ONNX model for AI detection
- `--tokenizer <path>`: Tokenizer for AI detection
- `--enable-reddit`: Enable Reddit scraper endpoints
- `--help`: Show help message

---

## Error Responses

All error responses follow this format:

```json
{
  "error": "Error message describing what went wrong"
}
```

**Common HTTP Status Codes:**
- `200 OK`: Success
- `400 Bad Request`: Invalid request body or missing required fields
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error (check logs)

---

## CORS

CORS is enabled for all origins (`*`). All endpoints support:
- `GET`, `POST`, `PUT`, `DELETE`, `OPTIONS`
- Headers: `Content-Type`, `Authorization`

---

## Rate Limiting

- Reddit API: 60 requests per minute (handled internally)
- No rate limiting on other endpoints (add if needed)

---

## New Features Summary (v1.1.0)

### ✅ Added Endpoints
1. **Reddit Scraper** (5 endpoints)
   - `/api/reddit/status` - Get scraper status
   - `/api/reddit/start` - Start automated scraping
   - `/api/reddit/stop` - Stop scraping
   - `/api/reddit/scrape` - Manual scrape trigger
   - `/api/reddit/items` - Get scraped Reddit items

2. **Chatbot with Railguard** (3 endpoints)
   - `/api/chat` - Send message with bidirectional moderation
   - `/api/chat/history` - Get chat history
   - `/api/chat/history` (DELETE) - Clear chat history

3. **AI Detection & Source Identification** (1 endpoint)
   - `/api/detect/ai` - Detect AI content and identify source model

### ✅ Enhanced Features
- Statistics endpoint now includes Reddit and chat data
- Content filtering by source type
- Automated moderation for scraped content
- Real-time content processing pipeline

---

## Testing Examples

### cURL Examples

**Start Reddit Scraper:**
```bash
curl -X POST http://localhost:8080/api/reddit/start \
  -H "Content-Type: application/json" \
  -d '{"subreddits": ["technology", "programming"], "interval": 300}'
```

**Send Chat Message:**
```bash
curl -X POST http://localhost:8080/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello, how are you?"}'
```

**Detect AI Content:**
```bash
curl -X POST http://localhost:8080/api/detect/ai \
  -H "Content-Type: application/json" \
  -d '{"text": "As an AI language model, I can help you with..."}'
```

**Get Statistics:**
```bash
curl http://localhost:8080/api/stats
```

---

## Support & Documentation

- **Main Documentation**: `/MIGRATION_COMPLETE.md`
- **Build Instructions**: `/backend/README.md`
- **Frontend Prompt**: `/LOVABLE_PROMPT.md`
- **Comparison**: `/COMPARISON.md`

---

**API Version:** 1.1.0  
**Last Updated:** December 17, 2024  
**Backend Status:** ✅ Production Ready
