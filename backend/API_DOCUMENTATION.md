# API Endpoint Documentation

## Base URL
```
http://localhost:8080/api
```

## Authentication
Currently no authentication required. To add API keys in the future:
```bash
export MODAI_HIVE_API_KEY="your_hive_api_key"
```

---

## Endpoints

### 1. Health Check

**GET** `/health`

Check if the server is running.

**Response:**
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "timestamp": 1734450000
}
```

---

### 2. Get All Content

**GET** `/content`

Retrieve all moderated content items.

**Query Parameters:**
- `filter` (optional): Filter by decision action (`allow`, `block`, `review`)
- `subreddit` (optional): Filter by subreddit name

**Example:**
```bash
curl "http://localhost:8080/api/content?filter=block&subreddit=news"
```

**Response:**
```json
[
  {
    "id": "uuid-here",
    "timestamp": "2025-12-17T12:00:00.000Z",
    "source": "reddit",
    "subreddit": "news",
    "author": "username",
    "content_type": "text",
    "text": "Content text here...",
    "ai_detection": {
      "model": "desklib/ai-text-detector-v1.01",
      "ai_score": 0.85,
      "label": "ai",
      "confidence": 0.70
    },
    "moderation": {
      "provider": "hive",
      "labels": {
        "sexual": 0.02,
        "violence": 0.01,
        "hate": 0.03,
        "drugs": 0.01
      }
    },
    "decision": {
      "auto_action": "review",
      "rule_id": "rule_001",
      "threshold_triggered": true
    },
    "schema_version": 1
  }
]
```

---

### 3. Get Single Content Item

**GET** `/content/{id}`

Retrieve a specific content item by ID.

**Response:** Same as single item above, or 404 if not found.

---

### 4. Moderate Text Content

**POST** `/moderate/text`

Submit text content for moderation.

**Request Body:**
```json
{
  "text": "Content to moderate",
  "subreddit": "mysubreddit",
  "author": "username"
}
```

**Response:** Full content item with moderation results (same structure as GET content).

**Example:**
```bash
curl -X POST http://localhost:8080/api/moderate/text \
  -H "Content-Type: application/json" \
  -d '{
    "text": "This is a test message",
    "subreddit": "test",
    "author": "testuser"
  }'
```

---

### 5. Moderate Image Content

**POST** `/moderate/image`

Submit an image for moderation.

**Content-Type:** `multipart/form-data`

**Form Fields:**
- `image` (file): Image file to moderate
- `subreddit` (string): Subreddit name

**Example:**
```bash
curl -X POST http://localhost:8080/api/moderate/image \
  -F "image=@/path/to/image.jpg" \
  -F "subreddit=pics"
```

**Response:** Full content item with moderation results.

---

### 6. Update Content Decision (Human Review)

**PUT** `/content/{id}/decision`

Update the decision for a content item (human review action).

**Request Body:**
```json
{
  "action": "allow",
  "reviewer": "moderator_name",
  "reason": "False positive - content is acceptable"
}
```

**Response:**
```json
{
  "success": true,
  "action_id": "1734450123"
}
```

**Example:**
```bash
curl -X PUT http://localhost:8080/api/content/uuid-here/decision \
  -H "Content-Type: application/json" \
  -d '{
    "action": "allow",
    "reviewer": "admin",
    "reason": "Reviewed and approved"
  }'
```

---

### 7. Get Statistics

**GET** `/stats`

Get aggregate statistics about moderated content.

**Response:**
```json
{
  "total_items": 150,
  "blocked": 25,
  "review": 40,
  "allowed": 85,
  "text_items": 120,
  "image_items": 30,
  "avg_ai_score": 0.35,
  "avg_sexual_score": 0.12,
  "avg_violence_score": 0.08
}
```

---

### 8. Export Data

**GET** `/export`

Export all content data to a file.

**Query Parameters:**
- `format` (required): Export format (`csv` or `json`)

**Example:**
```bash
# Export as CSV
curl "http://localhost:8080/api/export?format=csv"

# Export as JSON
curl "http://localhost:8080/api/export?format=json"
```

**Response:**
```
CSV export created: ./data/exports/1734450123.csv
```

The exported file is saved on the server. To download it, you can:
1. Access the file directly from the server
2. Extend the API to serve the file for download

---

## Error Responses

All endpoints return standard HTTP status codes:

- `200 OK`: Success
- `400 Bad Request`: Invalid request parameters
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error

**Error Response Format:**
```json
{
  "error": "Error message describing what went wrong"
}
```

---

## Data Flow

### Text Moderation Flow
```
1. Client POST /api/moderate/text
2. Backend creates ContentItem
3. LocalAIDetector analyzes text → AI score
4. HiveTextModerator analyzes text → moderation labels
5. RuleEngine evaluates rules → decision
6. Save to JsonlStorage
7. Return ContentItem with results
```

### Image Moderation Flow
```
1. Client POST /api/moderate/image (multipart)
2. Backend saves image to disk
3. Backend creates ContentItem with image_path
4. Load image bytes from disk
5. Compute SHA256 hash
6. Check ResultCache for cached results
7. If not cached:
   - HiveImageModerator analyzes image → labels
   - Cache the result
8. RuleEngine evaluates rules → decision
9. Save to JsonlStorage
10. Return ContentItem with results
```

---

## Rate Limiting

The backend includes rate limiting for external API calls:
- **Hive API**: 100 requests per 60 seconds
- Automatic queuing when rate limit reached
- Exponential backoff on failures (3 retries)

---

## Configuration

### Rules Configuration

Place rules in `config/rules.json`:

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
    },
    {
      "id": "rule_003",
      "name": "Hate Speech Block",
      "condition": "hate > 0.8",
      "action": "block",
      "subreddit": "",
      "enabled": true
    }
  ]
}
```

**Rule Conditions:**
- Supports `>`, `>=`, `<`, `<=`, `==` operators
- Can combine with `&&` (AND) and `||` (OR)
- Available fields: `ai_score`, `sexual`, `violence`, `hate`, `drugs`
- Examples:
  - `ai_score > 0.8`
  - `sexual > 0.7 && violence > 0.5`
  - `hate > 0.8 || drugs > 0.9`

---

## Example Frontend Integration

### React Example

```jsx
import React, { useState } from 'react';

function ModerateText() {
  const [text, setText] = useState('');
  const [result, setResult] = useState(null);
  
  const moderate = async () => {
    const response = await fetch('http://localhost:8080/api/moderate/text', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        text: text,
        subreddit: 'test',
        author: 'user'
      })
    });
    const data = await response.json();
    setResult(data);
  };
  
  return (
    <div>
      <textarea value={text} onChange={(e) => setText(e.target.value)} />
      <button onClick={moderate}>Moderate</button>
      {result && (
        <div>
          <p>Decision: {result.decision.auto_action}</p>
          <p>AI Score: {result.ai_detection.ai_score}</p>
        </div>
      )}
    </div>
  );
}
```

### Vue.js Example

```vue
<template>
  <div>
    <textarea v-model="text"></textarea>
    <button @click="moderate">Moderate</button>
    <div v-if="result">
      <p>Decision: {{ result.decision.auto_action }}</p>
      <p>AI Score: {{ result.ai_detection.ai_score }}</p>
    </div>
  </div>
</template>

<script>
export default {
  data() {
    return {
      text: '',
      result: null
    };
  },
  methods: {
    async moderate() {
      const response = await fetch('http://localhost:8080/api/moderate/text', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          text: this.text,
          subreddit: 'test',
          author: 'user'
        })
      });
      this.result = await response.json();
    }
  }
};
</script>
```

---

## Performance Tips

1. **Batch Requests**: Group multiple moderation requests when possible
2. **Caching**: Image moderation results are automatically cached by hash
3. **Filters**: Use query parameters to reduce response size
4. **Rate Limiting**: Be aware of Hive API rate limits (100/min)

---

## Security Considerations

### For Production Deployment:

1. **Add Authentication**: Implement JWT or OAuth2
2. **HTTPS**: Use TLS/SSL certificates
3. **CORS**: Configure proper CORS headers
4. **Input Validation**: Validate all user inputs
5. **File Upload Limits**: Set max file size for images
6. **Rate Limiting**: Implement per-client rate limiting
7. **API Keys**: Secure storage of Hive API keys

Example with authentication:
```cpp
// In ApiServer.cpp, add authentication middleware
svr.set_pre_routing_handler([](const auto& req, auto& res) {
    std::string auth = req.get_header_value("Authorization");
    if (!validateToken(auth)) {
        res.status = 401;
        res.set_content("{\"error\":\"Unauthorized\"}", "application/json");
        return httplib::Server::HandlerResponse::Handled;
    }
    return httplib::Server::HandlerResponse::Unhandled;
});
```

---

## Troubleshooting

### Server Won't Start

1. Check if port is already in use:
   ```bash
   lsof -i :8080
   ```

2. Check logs:
   ```bash
   tail -f data/logs/backend.log
   ```

### API Calls Failing

1. Verify server is running:
   ```bash
   curl http://localhost:8080/api/health
   ```

2. Check for CORS issues in browser console

3. Verify request format matches documentation

### Moderation Returns Empty Results

1. Check if Hive API key is set:
   ```bash
   echo $MODAI_HIVE_API_KEY
   ```

2. Check if rate limit exceeded in logs

3. Verify network connectivity to Hive API
