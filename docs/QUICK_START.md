# Quick Start Guide - ModAI Multi-Mode

## ✅ Build Successful!

Your ModAI application now has **4 powerful modes** ready to use!

---

## 🚀 Running the Application

```bash
cd /home/mayank/work_space/ModAI_cpp/build
./ModAI
```

---

## 📑 Available Modes

### 1. 📱 Reddit Scraper & Moderator (Original)
- Scrape and analyze Reddit posts/comments
- Auto-detect AI-generated content
- Hive API moderation

### 2. 💬 AI Chatbot with Railguard (NEW!)
- Chat with LLM (Ollama/OpenAI)
- Responses filtered by Hive API
- Blocks offensive content automatically

### 3. 📝 AI Text Detector (NEW!)
- Paste text to check if AI-generated
- Local ONNX model (fast, private)
- 0-100% confidence score

### 4. 🖼️ AI Image Detector (NEW!)
- Upload images to detect AI generation
- Uses Hive API computer vision
- Detects Stable Diffusion, DALL-E, etc.

---

## ⚙️ Setup Requirements

### For Chatbot Mode (Option A: Local LLM - Recommended)

```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Download a model
ollama pull llama2

# Run Ollama (in a separate terminal)
ollama serve
```

### For Chatbot Mode (Option B: OpenAI API)

Edit [src/ui/MainWindow.cpp](../src/ui/MainWindow.cpp) line ~168:
```cpp
chatbotPanel_->initialize(
    std::make_unique<QtHttpClient>(this),
    std::make_unique<HiveTextModerator>(...),
    "sk-YOUR_OPENAI_API_KEY_HERE",  // Add your key
    "https://api.openai.com/v1/chat/completions"  // OpenAI endpoint
);
```

Then edit [src/ui/ChatbotPanel.cpp](../src/ui/ChatbotPanel.cpp) line ~116:
```cpp
payload["model"] = "gpt-3.5-turbo";  // or "gpt-4"
```

### For AI Text Detector

Export the ONNX model (one-time setup):
```bash
cd /home/mayank/work_space/ModAI_cpp
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/data/models
```

This downloads and converts the Hugging Face model to ONNX format (~300MB).

### For All Modes

Set your Hive API key (you'll be prompted on first run):
- Get key at: https://thehive.ai/
- The app stores it securely

---

## 🎨 UI Overview

```
┌─────────────────────────────────────────────────┐
│  [Reddit Scraper] [Chatbot] [Text Det.] [Image] │ ← Tab Bar
├─────────────────────────────────────────────────┤
│                                                 │
│           MODE-SPECIFIC INTERFACE               │
│                                                 │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## 💡 Usage Examples

### Example 1: Test AI Text Detection

1. Click "📝 AI Text Detector" tab
2. Paste this text:
   ```
   Artificial intelligence has revolutionized the way we approach 
   complex problems. Machine learning algorithms can process vast 
   amounts of data, identifying patterns that humans might miss.
   ```
3. Click "Analyze Text"
4. View AI score (will show as potentially AI-generated)

### Example 2: Safe Chatbot

1. Click "💬 AI Chatbot (Railguard)" tab
2. Type: "Tell me a story"
3. Watch as AI generates response
4. Response is automatically checked by Hive API
5. If inappropriate → 🛑 Blocked

### Example 3: Image Analysis

1. Click "🖼️ AI Image Detector" tab
2. Click "Select Image"
3. Choose any image file
4. Click "Analyze Image"
5. See AI generation confidence

---

## 🔧 Configuration

### Chatbot Safety Threshold

Edit [src/ui/ChatbotPanel.cpp](../src/ui/ChatbotPanel.cpp) line ~249:
```cpp
const float threshold = 0.7f;  // Lower = stricter (0.5-0.9 recommended)
```

### AI Text Detection Sensitivity

Edit [src/detectors/LocalAIDetector.cpp](../src/detectors/LocalAIDetector.cpp) constructor:
```cpp
threshold_ = 0.5f;  // Adjust sensitivity (0.3-0.7 typical)
```

---

## 📊 Performance

- **Chatbot**: 1-5 seconds per response
- **Text Detection**: 100-500ms (local)
- **Image Detection**: 1-2 seconds (API)
- **Reddit Scraping**: Rate-limited (60 req/min)

---

## 🐛 Troubleshooting

### Chatbot says "LLM Error"
```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags

# If not, start it
ollama serve
```

### Text Detector shows "not available"
```bash
# Export the ONNX model
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/data/models

# Check if files exist
ls ~/.local/share/ModAI/data/models/
# Should see: ai_detector.onnx, tokenizer files
```

### Image Detector not working
- Ensure Hive API key is set
- Check logs: `~/.local/share/ModAI/data/logs/system.log`

---

## 📝 Logs

Application logs are saved to:
```
~/.local/share/ModAI/data/logs/system.log
```

View in real-time:
```bash
tail -f ~/.local/share/ModAI/data/logs/system.log
```

---

## 🎯 Next Steps

1. **Test all modes** to ensure everything works
2. **Set API keys** (Hive required, OpenAI optional)
3. **Export ONNX model** for text detection
4. **Install Ollama** for local chatbot (optional)
5. **Customize thresholds** for your use case

---

## 📚 Documentation

- **Full Feature Guide**: [docs/MULTI_MODE_FEATURES.md](MULTI_MODE_FEATURES.md)
- **Build Instructions**: [BUILD.md](../BUILD.md)
- **Local AI Setup**: [docs/LOCAL_AI_SETUP.md](LOCAL_AI_SETUP.md)

---

## ✨ What's New

### Architecture
- ✅ Tabbed multi-mode interface
- ✅ Shared detector/moderator services
- ✅ Clean panel-based design

### New Panels
- ✅ `ChatbotPanel` - LLM with safety railguard
- ✅ `AITextDetectorPanel` - Local AI detection
- ✅ `AIImageDetectorPanel` - Image AI detection

### New Files Created
```
include/ui/
  - ChatbotPanel.h
  - AITextDetectorPanel.h
  - AIImageDetectorPanel.h

src/ui/
  - ChatbotPanel.cpp
  - AITextDetectorPanel.cpp
  - AIImageDetectorPanel.cpp

docs/
  - MULTI_MODE_FEATURES.md
  - QUICK_START.md (this file)
```

---

## 🤝 Support

If you encounter issues:
1. Check logs
2. Verify API keys
3. Ensure Ollama is running (for chatbot)
4. Check ONNX model is exported (for text detection)

---

**Enjoy your powerful multi-mode AI content analysis tool! 🚀**
