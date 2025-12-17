# Lovable AI Prompt: ModAI Content Moderation Dashboard

## Project Overview

Create a professional, modern web dashboard for **ModAI** - an AI-powered content moderation platform with integrated Reddit scraping, chatbot Railguard, and AI detection capabilities. The dashboard should provide a clean, intuitive interface for content moderators, community managers, and AI safety teams.

## Backend Architecture (Already Implemented)

### Technology Stack
- **Language**: C++17
- **HTTP Server**: cpp-httplib (REST API)
- **HTTP Client**: libcurl
- **AI Detection**: ONNX Runtime (local inference)
- **Content Moderation**: Hive AI API
- **Storage**: JSONL (file-based)
- **Encryption**: OpenSSL (AES-256-CBC)

### API Base URL
```
http://localhost:8080/api
```

### Core Components
1. **Moderation Engine**: Analyzes text and images for inappropriate content
2. **AI Detection**: Identifies AI-generated content and source (GPT, Claude, Gemini, etc.)
3. **Rule Engine**: Applies custom moderation rules
4. **Reddit Scraper**: Automated subreddit monitoring with moderation
5. **Chatbot Railguard**: Real-time chat moderation for preventing harmful AI responses
6. **Result Cache**: Performance optimization
7. **Storage**: Persistent content storage with JSONL

## Required Frontend Features

### 1. **Reddit Scraper Dashboard** 🔍

#### Purpose
Monitor and moderate content scraped from specified subreddits in real-time.

#### Key Features
- **Subreddit Configuration Panel**
  - Add/remove subreddits to monitor
  - Set scraping interval (default 5 minutes)
  - Start/stop scraping with visual status indicator
  - Display active subreddits with item counts

- **Live Content Feed**
  - Real-time display of scraped posts and comments
  - Filter by subreddit, content type (text/image), moderation status
  - Show thumbnails for image posts
  - Display Reddit metadata: author, subreddit, score, timestamp, permalink

- **Moderation Analysis Display**
  - AI-generated probability score with visual indicator (progress bar/gauge)
  - Content moderation scores:
    - Sexual content score
    - Violence score
    - Hate speech score
    - Harassment score
  - Automated decision: Allow/Review/Block with color coding
  - Rule evaluation results

- **Manual Review Interface**
  - Click to expand full content details
  - Human reviewer can override automated decision
  - Add notes for review decisions
  - Assign to reviewer with timestamp

#### API Endpoints to Use

```javascript
// Get scraper status
GET /api/reddit/status
Response: { is_running: boolean, subreddits: string[] }

// Start scraping
POST /api/reddit/start
Body: { subreddits: string[], interval: number }
Response: { success: boolean, message: string, ... }

// Stop scraping
POST /api/reddit/stop
Response: { success: boolean, message: string }

// Manual scrape trigger
POST /api/reddit/scrape
Body: { subreddits: string[] }
Response: { success: boolean, items_scraped: number, items: ContentItem[] }

// Get scraped items
GET /api/reddit/items?subreddit=<name>
Response: ContentItem[]

// Get all content with filters
GET /api/content?filter=<decision>&subreddit=<name>&type=<text|image>
Response: ContentItem[]

// Update decision
PUT /api/content/{id}/decision
Body: { decision: string, reviewer: string, notes: string }
Response: { success: boolean, content: ContentItem }
```

#### UI/UX Recommendations
- Use a card-based layout for content items
- Color-coded badges for moderation status (green=allow, yellow=review, red=block)
- Real-time updates using polling (every 5 seconds) or implement SSE if needed
- Responsive design for mobile moderators
- Dark mode support for long moderation sessions

---

### 2. **Chatbot with Railguard** 💬

#### Purpose
Provide a demo chatbot interface with built-in content moderation that prevents generation and display of offensive content.

#### Key Features
- **Chat Interface**
  - Clean, modern chat UI (similar to ChatGPT/Claude)
  - Message bubbles for user and assistant
  - Typing indicator while processing
  - Timestamp for each message
  - Auto-scroll to latest message

- **Real-time Moderation**
  - User messages are checked BEFORE processing
  - If blocked, show warning: "Your message contains inappropriate content and cannot be processed"
  - Display moderation scores for transparency
  - AI responses are ALSO checked before display
  - If AI response is blocked, show: "I apologize, but my response was flagged by safety systems"

- **Moderation Transparency Panel**
  - Show moderation scores for both user input and AI output
  - Display which categories triggered blocks (sexual, violence, hate, etc.)
  - Optional toggle to hide/show technical details

- **Chat History**
  - View past conversations
  - Filter blocked vs. allowed messages
  - Export chat logs
  - Clear history button

#### API Endpoints to Use

```javascript
// Send message with Railguard
POST /api/chat
Body: { message: string }
Response: {
  id: string,
  message: string,  // AI response (if not blocked)
  blocked: boolean,
  reason?: string,  // If blocked
  timestamp: number,
  moderation_score: {
    user_message: { sexual: number, violence: number, hate: number },
    ai_response: { sexual: number, violence: number, hate: number }
  }
}

// Get chat history
GET /api/chat/history?limit=50
Response: ChatMessage[]

// Clear chat history
DELETE /api/chat/history
Response: { success: boolean, message: string }
```

#### UI/UX Recommendations
- Modern glassmorphism design for chat bubbles
- Red warning icon/border for blocked messages
- Expandable moderation details (click to see scores)
- Settings panel for adjusting moderation sensitivity (future feature)
- Mobile-first responsive design

---

### 3. **AI Detection & Source Identification** 🤖

#### Purpose
Analyze text or images to determine if they're AI-generated and identify the likely source model.

#### Key Features
- **Input Methods**
  - Text area for pasting/typing content
  - File upload for images (drag-and-drop support)
  - URL input for analyzing online content (future)

- **Analysis Display**
  - **AI Probability Score**: Large, prominent percentage (0-100%)
  - **Is AI Generated?**: Clear Yes/No with confidence level
  - **Detected Source**: Display likely model
    - ChatGPT (OpenAI)
    - Claude (Anthropic)
    - Gemini (Google)
    - Generic LLM (unknown specific model)
  - **Indicators**: List of detection clues
    - "Contains Claude-specific phrasing"
    - "Well-structured with paragraphs"
    - "Formal sentence structure"

- **Text Analysis Metrics**
  - Text length
  - Sentence count
  - Average word length
  - Paragraph structure analysis

- **Model Information**
  - Which detection model was used
  - Confidence score
  - Processing time

- **History of Analyzed Content**
  - Save analyses for comparison
  - Export results as CSV/JSON
  - Filter by AI score range

#### API Endpoints to Use

```javascript
// Detect AI-generated text
POST /api/detect/ai
Body: { text: string }
Response: {
  ai_score: number,          // 0.0 to 1.0
  ai_confidence: number,     // 0.0 to 1.0
  is_ai_generated: boolean,
  detected_source: string,   // "ChatGPT (OpenAI)", "Claude (Anthropic)", etc.
  indicators: string[],      // List of detection clues
  model_used: string,        // Which detection model was used
  analysis: {
    text_length: number,
    sentence_count: number,
    avg_word_length: number
  }
}

// For images, use moderation endpoint
POST /api/moderate/image
Body: multipart/form-data with 'image' field
Response: ContentItem with ai_detection and moderation data
```

#### UI/UX Recommendations
- Split-screen layout: Input on left, results on right
- Large, animated circular progress for AI score
- Use color gradients: Green (human) → Yellow (unclear) → Red (AI)
- Icons for different AI models (OpenAI logo, Anthropic logo, etc.)
- "Copy to clipboard" for analysis results
- Export analysis report as PDF (optional)

---

### 4. **Main Dashboard** 📊

#### Purpose
Centralized overview of all moderation activities and statistics.

#### Key Features
- **Statistics Cards**
  - Total items processed
  - Blocked items count
  - Items under review
  - Allowed items
  - AI-generated content percentage
  - Reddit items scraped

- **Charts and Visualizations**
  - Time-series chart: Content volume over time
  - Pie chart: Decision breakdown (allow/block/review)
  - Bar chart: Content type distribution (text/image)
  - Line chart: Average moderation scores over time
  - Subreddit breakdown (if Reddit scraper active)

- **Recent Activity Feed**
  - Latest moderated items
  - Recent decisions
  - Flagged content requiring review

- **Quick Actions**
  - Jump to Reddit scraper
  - Open chatbot
  - Run AI detection
  - Export all data

#### API Endpoints to Use

```javascript
// Get comprehensive statistics
GET /api/stats
Response: {
  total_items: number,
  blocked: number,
  review: number,
  allowed: number,
  text_items: number,
  image_items: number,
  ai_generated: number,
  reddit_items: number,
  avg_ai_score: number,
  avg_sexual_score: number,
  avg_violence_score: number,
  subreddit_breakdown: { [subreddit: string]: number },
  chat_messages: number
}

// Health check
GET /api/health
Response: { status: string, version: string, timestamp: number }
```

---

### 5. **Content Review Interface** 👁️

#### Purpose
Detailed review interface for content flagged for human review.

#### Key Features
- **Content Display**
  - Full text content
  - Image viewer with zoom
  - Source information (Reddit, API, chatbot)
  - Timestamp and metadata

- **Moderation Scores Panel**
  - All category scores with visual bars
  - AI detection details
  - Rule evaluation results
  - Automated decision with reasoning

- **Review Actions**
  - Approve (override to allow)
  - Block (confirm block)
  - Escalate (send to senior reviewer)
  - Add notes
  - Assign tags/categories

- **Review Queue**
  - Filter by priority
  - Sort by timestamp, AI score, source
  - Batch review actions
  - Keyboard shortcuts for efficiency

#### API Endpoints to Use

```javascript
// Get items needing review
GET /api/content?filter=review
Response: ContentItem[]

// Get specific item
GET /api/content/{id}
Response: ContentItem

// Update decision
PUT /api/content/{id}/decision
Body: { decision: "allow" | "block", reviewer: string, notes: string }
Response: { success: boolean, content: ContentItem }
```

---

## Data Models

### ContentItem
```typescript
interface ContentItem {
  id: string;
  content_type: "text" | "image" | "both";
  text?: string;           // Primary text field
  text_content?: string;   // Alias for text (both supported)
  image_path?: string;
  source: "reddit" | "api" | "chatbot";
  subreddit: string;       // Direct field (not in metadata)
  author?: string;         // Direct field (not in metadata)
  timestamp: string;       // ISO-8601 string format
  content_id?: string;     // Generic content ID
  post_id?: string;        // Reddit post ID
  
  // AI Detection Results
  ai_detection: {
    label: "ai_generated" | "human";
    ai_score: number;  // 0.0 to 1.0
    confidence: number;
    model: string;
  };
  
  // Moderation Results
  moderation: {
    provider: "hive";
    labels: {
      sexual: number;
      violence: number;
      hate: number;
      drugs: number;
      harassment: number;
      self_harm: number;
      illicit: number;
      additional_labels?: { [key: string]: number };
    };
  };
  
  // Decision
  decision: {
    auto_action: "allow" | "block" | "review";
    rule_id: string;
    threshold_triggered: boolean;
    human_decision: string;      // Can be empty string
    human_reviewer: string;      // Can be empty string
    human_notes: string;         // Can be empty string
    human_review_timestamp: number;  // Unix timestamp, 0 if not reviewed
  };
  
  // Metadata (additional fields, especially for Reddit)
  metadata: {
    [key: string]: string;
    // Common Reddit fields: title, score, permalink
  };
  
  schema_version: number;
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

## Technical Requirements

### Frontend Technology Stack (Recommended)
- **Framework**: React 18+ with TypeScript or Next.js 14+
- **UI Library**: 
  - Tailwind CSS for styling
  - shadcn/ui or Material-UI for components
  - Framer Motion for animations
- **State Management**: Zustand or React Context API
- **API Client**: Axios or Fetch API
- **Charts**: Recharts or Chart.js
- **Icons**: Lucide React or Heroicons

### Design System
- **Colors**:
  - Primary: Blue (#3B82F6)
  - Success: Green (#10B981)
  - Warning: Yellow (#F59E0B)
  - Danger: Red (#EF4444)
  - Dark Mode: Gray (#1F2937 background, #111827 cards)

- **Typography**:
  - Headings: Inter or Poppins (Bold)
  - Body: Inter or system-ui (Regular)
  - Code: JetBrains Mono or Fira Code

- **Layout**:
  - Sidebar navigation (collapsible on mobile)
  - Top bar with user profile and settings
  - Responsive breakpoints: mobile (< 640px), tablet (< 1024px), desktop (>= 1024px)

### Navigation Structure
```
/ (Main Dashboard)
├── /reddit (Reddit Scraper Dashboard)
├── /chatbot (Chatbot with Railguard)
├── /ai-detection (AI Detection & Source ID)
├── /review (Content Review Queue)
├── /content (All Content Browse)
├── /settings (Configuration)
└── /export (Data Export)
```

---

## Implementation Guidelines

### 1. API Integration
```typescript
// Example API utility
const API_BASE = 'http://localhost:8080/api';

export const api = {
  reddit: {
    getStatus: () => fetch(`${API_BASE}/reddit/status`).then(r => r.json()),
    start: (subreddits: string[], interval: number) => 
      fetch(`${API_BASE}/reddit/start`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ subreddits, interval })
      }).then(r => r.json()),
    stop: () => 
      fetch(`${API_BASE}/reddit/stop`, { method: 'POST' }).then(r => r.json()),
    scrapeOnce: (subreddits: string[]) =>
      fetch(`${API_BASE}/reddit/scrape`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ subreddits })
      }).then(r => r.json()),
    getItems: (subreddit?: string) =>
      fetch(`${API_BASE}/reddit/items${subreddit ? `?subreddit=${subreddit}` : ''}`).then(r => r.json())
  },
  
  chat: {
    send: (message: string) =>
      fetch(`${API_BASE}/chat`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message })
      }).then(r => r.json()),
    getHistory: (limit = 50) =>
      fetch(`${API_BASE}/chat/history?limit=${limit}`).then(r => r.json()),
    clearHistory: () =>
      fetch(`${API_BASE}/chat/history`, { method: 'DELETE' }).then(r => r.json())
  },
  
  detect: {
    analyzeText: (text: string) =>
      fetch(`${API_BASE}/detect/ai`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text })
      }).then(r => r.json())
  },
  
  content: {
    getAll: (filters?: { filter?: string, subreddit?: string, type?: string }) => {
      const params = new URLSearchParams(filters as any);
      return fetch(`${API_BASE}/content?${params}`).then(r => r.json());
    },
    getById: (id: string) =>
      fetch(`${API_BASE}/content/${id}`).then(r => r.json()),
    updateDecision: (id: string, decision: string, reviewer: string, notes?: string) =>
      fetch(`${API_BASE}/content/${id}/decision`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ decision, reviewer, notes })
      }).then(r => r.json())
  },
  
  moderate: {
    text: (text: string, metadata?: any) =>
      fetch(`${API_BASE}/moderate/text`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text, metadata })
      }).then(r => r.json()),
    image: (file: File) => {
      const formData = new FormData();
      formData.append('image', file);
      return fetch(`${API_BASE}/moderate/image`, {
        method: 'POST',
        body: formData
      }).then(r => r.json());
    }
  },
  
  stats: {
    get: () => fetch(`${API_BASE}/stats`).then(r => r.json())
  },
  
  export: {
    getData: (format: 'csv' | 'json') =>
      fetch(`${API_BASE}/export?format=${format}`).then(r => r.text())
  }
};
```

### 2. Real-time Updates
Implement polling for real-time updates:
```typescript
// Poll every 5 seconds for Reddit items
useEffect(() => {
  const interval = setInterval(() => {
    api.reddit.getItems().then(setItems);
  }, 5000);
  return () => clearInterval(interval);
}, []);
```

### 3. Error Handling
Always implement proper error handling:
```typescript
try {
  const response = await api.reddit.start(subreddits, interval);
  if (response.success) {
    showToast('Scraper started successfully', 'success');
  }
} catch (error) {
  console.error('Failed to start scraper:', error);
  showToast('Failed to start scraper', 'error');
}
```

---

## Priority Features for MVP

### Phase 1 (Core Functionality)
1. ✅ Main Dashboard with statistics
2. ✅ Reddit Scraper Dashboard (basic)
3. ✅ Chatbot with Railguard
4. ✅ AI Detection interface

### Phase 2 (Enhanced Features)
5. Content Review Queue
6. Advanced filtering and search
7. Export functionality
8. User authentication (future)

### Phase 3 (Polish)
9. Dark mode
10. Mobile optimization
11. Keyboard shortcuts
12. Batch operations

---

## Testing Requirements

- Test all API endpoints with sample data
- Ensure error states are handled gracefully
- Test on multiple screen sizes
- Verify real-time updates work correctly
- Load test with large datasets (1000+ items)

---

## Performance Considerations

- Implement pagination for large datasets (100 items per page)
- Use virtual scrolling for long lists
- Lazy load images
- Debounce search inputs
- Cache API responses where appropriate
- Optimize re-renders with React.memo and useMemo

---

## Accessibility Requirements

- WCAG 2.1 Level AA compliance
- Keyboard navigation support
- Screen reader friendly
- High contrast mode support
- Focus indicators
- Semantic HTML

---

## Additional Notes

### Backend Server Commands
```bash
# Start server with all features
cd backend
./build/ModAI_Backend --port 8080 --enable-reddit

# Start with AI detection
./build/ModAI_Backend --model ./models/ai_detector.onnx --tokenizer ./models/tokenizer.json

# Environment variables
export MODAI_HIVE_API_KEY="your_hive_key"
export MODAI_REDDIT_CLIENT_ID="your_reddit_client_id"
export MODAI_REDDIT_CLIENT_SECRET="your_reddit_secret"
```

### Development Tips
1. The C++ backend is already fully implemented and tested
2. All API endpoints return JSON
3. CORS is enabled for all origins (`*`)
4. The backend runs on `http://localhost:8080` by default
5. Use Postman/curl to test endpoints before integrating
6. Check `backend/API_DOCUMENTATION.md` for detailed API specs

---

## Expected Output

A fully functional, production-ready web dashboard that:
- ✨ Looks professional and modern
- 🚀 Performs smoothly with large datasets
- 📱 Works perfectly on mobile and desktop
- 🎨 Has a cohesive design system
- 🔒 Handles errors gracefully
- 🌙 Supports dark mode
- ♿ Is accessible to all users

---

## Final Checklist

- [ ] All 5 main views implemented (Dashboard, Reddit, Chatbot, AI Detection, Review)
- [ ] All API endpoints integrated
- [ ] Real-time updates working
- [ ] Error handling implemented
- [ ] Responsive design verified
- [ ] Dark mode functional
- [ ] Loading states added
- [ ] Toast notifications for actions
- [ ] Export functionality working
- [ ] Code is clean and well-documented

---

**Ready to build an amazing content moderation dashboard! 🚀**
