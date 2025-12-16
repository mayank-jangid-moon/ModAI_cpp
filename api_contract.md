# API Contract

## Base URL
`http://localhost:8080/api/v1`

## Content Types
*   Requests: `application/json`
*   Responses: `application/json`

## Endpoints

### 1. System

#### `GET /health`
Returns the system health status.

**Response:**
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime": 12345
}
```

#### `GET /status`
Returns the current application state (scraping status).

**Response:**
```json
{
  "scraping_active": true,
  "current_subreddit": "technology",
  "items_processed": 150,
  "queue_size": 2
}
```

### 2. Scraper

#### `POST /scraper/start`
Starts the Reddit scraper.

**Request:**
```json
{
  "subreddit": "technology"
}
```

**Response:**
```json
{
  "status": "started",
  "subreddit": "technology"
}
```

**Errors:**
*   `400 Bad Request`: Missing subreddit.
*   `409 Conflict`: Scraping already active.

#### `POST /scraper/stop`
Stops the Reddit scraper.

**Response:**
```json
{
  "status": "stopped"
}
```

### 3. Content Items

#### `GET /items`
Retrieves a paginated list of content items.

**Query Parameters:**
*   `page`: Page number (default: 1)
*   `limit`: Items per page (default: 50)
*   `status`: Filter by status (`all`, `blocked`, `review`, `allowed`)
*   `search`: Search query string

**Response:**
```json
{
  "data": [
    {
      "id": "t3_12345",
      "timestamp": "2023-10-27T10:00:00Z",
      "subreddit": "technology",
      "content_type": "text",
      "text": "Sample post content...",
      "decision": {
        "auto_action": "block",
        "rule_id": "rule_1"
      },
      "moderation": {
        "labels": {
          "hate": 0.95
        }
      }
    }
  ],
  "pagination": {
    "current_page": 1,
    "total_pages": 10,
    "total_items": 500
  }
}
```

#### `GET /items/:id`
Retrieves details for a specific item.

**Response:**
```json
{
  "id": "t3_12345",
  "timestamp": "2023-10-27T10:00:00Z",
  "source": "reddit",
  "subreddit": "technology",
  "author": "user123",
  "content_type": "text",
  "text": "Full post content...",
  "ai_detection": {
    "model": "local_v1",
    "ai_score": 0.1,
    "label": "human"
  },
  "moderation": {
    "provider": "hive",
    "labels": {
      "sexual": 0.01,
      "hate": 0.0
    }
  },
  "decision": {
    "auto_action": "allow",
    "rule_id": "default"
  }
}
```

#### `POST /items/:id/decision`
Overrides the decision for an item.

**Request:**
```json
{
  "action": "block",
  "reason": "Manual override by user"
}
```

**Response:**
```json
{
  "id": "t3_12345",
  "status": "updated",
  "new_decision": "block"
}
```

### 4. Statistics

#### `GET /stats`
Returns dashboard statistics.

**Response:**
```json
{
  "total_scanned": 1000,
  "flagged_count": 50,
  "action_needed": 5,
  "ai_generated_count": 20
}
```
