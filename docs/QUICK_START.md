# Quick Start Guide

## Running the Application

After successful build, navigate to the build directory and execute:

```bash
cd /home/mayank/work_space/ModAI_cpp/build
./ModAI
```

## Application Modes

ModAI provides four distinct operational modes accessible through the tab interface:

### 1. Reddit Scraper & Moderator
- Automated scraping and analysis of Reddit posts and comments
- AI-generated content detection
- Hive API-based content moderation

### 2. AI Chatbot with Content Filtering
- Integration with LLM services (Ollama/OpenAI)
- Automatic response filtering through Hive API
- Real-time blocking of inappropriate content

### 3. AI Text Detector
- Analysis of text content for AI-generation detection
- Local ONNX model execution for privacy and speed
- Confidence scoring from 0-100%

### 4. AI Image Detector
- Detection of AI-generated images
- Identification of generation sources (Stable Diffusion, DALL-E, etc.)
- Hive API computer vision integration

## Configuration Requirements

### Chatbot Mode Setup (Option A: Local LLM)

Install and configure Ollama for local LLM execution:

```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Download a language model
ollama pull llama2

# Start the Ollama service
ollama serve
```

### Chatbot Mode Setup (Option B: OpenAI API)

Configure OpenAI API integration by editing src/ui/MainWindow.cpp (line ~168):

```cpp
chatbotPanel_->initialize(
    std::make_unique<QtHttpClient>(this),
    std::make_unique<HiveTextModerator>(...),
    "sk-YOUR_OPENAI_API_KEY_HERE",
    "https://api.openai.com/v1/chat/completions"
);
```

Update the model selection in src/ui/ChatbotPanel.cpp (line ~116):

```cpp
payload["model"] = "gpt-3.5-turbo";  // or "gpt-4"
```

### AI Text Detector Setup

Export the ONNX model (one-time setup required):

```bash
cd /home/mayank/work_space/ModAI_cpp
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/data/models
```

This process downloads and converts the Hugging Face model to ONNX format (approximately 300MB).

### API Key Configuration

Configure your Hive API key for content moderation:
- Obtain an API key from https://thehive.ai/
- The application will prompt for the key on first run
- Keys are stored securely in the application configuration

## User Interface Overview

Tab-based interface with four modes accessible from the top navigation bar.

## Usage Examples

### Example 1: AI Text Detection

1. Navigate to the "AI Text Detector" tab
2. Paste sample text
3. Click "Analyze Text"
4. Review the AI confidence score

### Example 2: Chatbot with Content Filtering

1. Navigate to the "AI Chatbot" tab
2. Enter a message prompt
3. Wait for AI response generation
4. The system automatically validates content through Hive API
5. Inappropriate content is blocked before display

### Example 3: Image Analysis

1. Navigate to the "AI Image Detector" tab
2. Click "Select Image" and choose an image file
3. Click "Analyze Image"
4. Review the AI generation confidence assessment

## Advanced Configuration

### Chatbot Safety Threshold

Adjust the content filtering threshold in src/ui/ChatbotPanel.cpp (line ~249):

```cpp
const float threshold = 0.7f;  // Range: 0.5-0.9 (lower = stricter filtering)
```

### AI Text Detection Sensitivity

Modify detection sensitivity in src/detectors/LocalAIDetector.cpp:

```cpp
threshold_ = 0.5f;  // Range: 0.3-0.7 (typical values)
```

## Performance Characteristics

- Chatbot Response Time: 1-5 seconds per interaction
- Text Detection: 100-500ms (local processing)
- Image Detection: 1-2 seconds (API-based)
- Reddit Scraping: Rate-limited to 60 requests per minute

## Troubleshooting

### Chatbot Connection Issues

Verify Ollama service status:
```bash
curl http://localhost:11434/api/tags
ollama serve
```

### Text Detector Unavailability

Export the required ONNX model:
```bash
python3 scripts/export_model_to_onnx.py --output ~/.local/share/ModAI/data/models
ls ~/.local/share/ModAI/data/models/
```

### Image Detector Failures

- Verify Hive API key configuration
- Review application logs at ~/.local/share/ModAI/data/logs/system.log

## Logging

Application logs are stored at:
```
~/.local/share/ModAI/data/logs/system.log
```

Monitor logs in real-time:
```bash
tail -f ~/.local/share/ModAI/data/logs/system.log
```

## Next Steps

1. Test each operational mode to verify functionality
2. Configure required API keys
3. Export ONNX model for text detection
4. Install Ollama for local chatbot functionality (optional)
5. Customize thresholds based on your requirements

## Additional Documentation

- Full Feature Documentation: MULTI_MODE_FEATURES.md
- Build Instructions: BUILD.md
- Local AI Configuration: LOCAL_AI_SETUP.md

## Support

For issues encountered during operation:
1. Review application logs
2. Verify API key configuration
3. Ensure Ollama service is running (for chatbot mode)
4. Confirm ONNX model export (for text detection)
