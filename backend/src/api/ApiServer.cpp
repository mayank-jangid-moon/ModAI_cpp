#include "api/ApiServer.h"
#include "utils/Logger.h"
#include "export/Exporter.h"
#include "network/CurlHttpClient.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <random>

using json = nlohmann::json;

namespace ModAI {

ApiServer::ApiServer(std::shared_ptr<ModerationEngine> moderationEngine,
                     std::shared_ptr<Storage> storage,
                     std::shared_ptr<RedditScraper> redditScraper,
                     int port,
                     const std::string& dataPath)
    : moderationEngine_(moderationEngine)
    , storage_(storage)
    , redditScraper_(redditScraper)
    , port_(port)
    , dataPath_(dataPath) {
}

std::string generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string id;
    for (int i = 0; i < 16; ++i) {
        id += hex[dis(gen)];
    }
    return id;
}

void ApiServer::start() {
    httplib::Server svr;
    
    Logger::info("Starting API server on port " + std::to_string(port_));
    
    // CORS middleware
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });
    
    // Handle OPTIONS for CORS preflight
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });
    
    // ===== HEALTH & INFO ENDPOINTS =====
    
    // Health check endpoint
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", "healthy"},
            {"version", "1.0.0"},
            {"timestamp", std::time(nullptr)}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    // ===== CONTENT MANAGEMENT ENDPOINTS =====
    
    // Get all content items
    svr.Get("/api/content", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto items = storage_->loadAllContent();
            
            // Apply filters from query params
            std::string filter = req.get_param_value("filter");
            std::string subreddit = req.get_param_value("subreddit");
            std::string contentType = req.get_param_value("type");
            
            json response = json::array();
            for (const auto& item : items) {
                if (!filter.empty() && item.decision.auto_action != filter) continue;
                if (!subreddit.empty() && item.subreddit != subreddit) continue;
                if (!contentType.empty() && item.content_type != contentType) continue;
                
                response.push_back(json::parse(item.toJson()));
            }
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error loading content: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Get single content item by ID
    svr.Get(R"(/api/content/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        
        try {
            auto items = storage_->loadAllContent();
            for (const auto& item : items) {
                if (item.id == id) {
                    res.set_content(item.toJson(), "application/json");
                    return;
                }
            }
            
            json error = {{"error", "Content not found"}};
            res.status = 404;
            res.set_content(error.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error loading content: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // ===== MODERATION ENDPOINTS =====
    
    // Moderate text content
    svr.Post("/api/moderate/text", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("text")) {
                json error = {{"error", "Missing required field: text"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            ContentItem item;
            item.id = generateId();
            item.text_content = body["text"].get<std::string>();
            item.content_type = "text";
            item.source = body.value("source", "api");
            item.timestamp = std::time(nullptr);
            
            if (body.contains("metadata")) {
                for (auto& [key, val] : body["metadata"].items()) {
                    item.metadata[key] = val.dump();
                }
            }
            
            moderationEngine_->processItem(item);
            storage_->saveContent(item);
            
            res.set_content(item.toJson(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error moderating text: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Moderate image content
    svr.Post("/api/moderate/image", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            if (!req.is_multipart_form_data()) {
                json error = {{"error", "Expected multipart/form-data"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            auto file = req.get_file_value("image");
            std::string filename = dataPath_ + "/uploads/" + generateId() + "_" + file.filename;
            
            std::ofstream ofs(filename, std::ios::binary);
            ofs << file.content;
            ofs.close();
            
            ContentItem item;
            item.id = generateId();
            item.image_path = filename;
            item.content_type = "image";
            item.source = "api";
            item.timestamp = std::time(nullptr);
            
            moderationEngine_->processItem(item);
            storage_->saveContent(item);
            
            res.set_content(item.toJson(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error moderating image: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Update content decision (human review)
    svr.Put(R"(/api/content/([^/]+)/decision)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("decision")) {
                json error = {{"error", "Missing required field: decision"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            auto items = storage_->loadAllContent();
            bool found = false;
            
            for (auto& item : items) {
                if (item.id == id) {
                    item.decision.human_decision = body["decision"].get<std::string>();
                    item.decision.human_reviewer = body.value("reviewer", "unknown");
                    item.decision.human_review_timestamp = std::time(nullptr);
                    
                    if (body.contains("notes")) {
                        item.decision.human_notes = body["notes"].get<std::string>();
                    }
                    
                    storage_->saveContent(item);
                    found = true;
                    
                    json response = {
                        {"success", true},
                        {"message", "Decision updated"},
                        {"content", json::parse(item.toJson())}
                    };
                    res.set_content(response.dump(), "application/json");
                    break;
                }
            }
            
            if (!found) {
                json error = {{"error", "Content not found"}};
                res.status = 404;
                res.set_content(error.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            Logger::error("Error updating decision: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // ===== CHATBOT ENDPOINTS =====
    
    // Chat endpoint with Railguard
    svr.Post("/api/chat", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("message")) {
                json error = {{"error", "Missing required field: message"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            std::string userMessage = body["message"].get<std::string>();
            
            // First, moderate the user's message with Railguard
            ContentItem userContent;
            userContent.id = generateId();
            userContent.text_content = userMessage;
            userContent.content_type = "text";
            userContent.source = "chatbot";
            userContent.timestamp = std::time(nullptr);
            
            moderationEngine_->processItem(userContent);
            
            ChatMessage userMsg;
            userMsg.id = generateId();
            userMsg.role = "user";
            userMsg.content = userMessage;
            userMsg.timestamp = std::time(nullptr);
            userMsg.was_blocked = false;
            
            // Check if user message should be blocked
            if (userContent.decision.auto_action == "block") {
                userMsg.was_blocked = true;
                userMsg.block_reason = "Message contains inappropriate content";
                
                json response = {
                    {"id", userMsg.id},
                    {"blocked", true},
                    {"reason", userMsg.block_reason},
                    {"moderation_details", {
                        {"sexual", userContent.moderation.labels.sexual},
                        {"violence", userContent.moderation.labels.violence},
                        {"hate", userContent.moderation.labels.hate},
                        {"harassment", userContent.moderation.labels.harassment}
                    }}
                };
                
                chatHistory_.push_back(userMsg);
                res.set_content(response.dump(), "application/json");
                return;
            }
            
            chatHistory_.push_back(userMsg);
            
            // Generate AI response (in a real implementation, call your LLM API here)
            std::string aiResponse = "I'm a demo assistant with Railguard protection. In a production environment, this would be replaced with actual LLM API calls (OpenAI, Anthropic, etc.). Your message was approved by moderation.";
            
            // Moderate the AI's response before sending
            ContentItem aiContent;
            aiContent.id = generateId();
            aiContent.text_content = aiResponse;
            aiContent.content_type = "text";
            aiContent.source = "chatbot";
            aiContent.timestamp = std::time(nullptr);
            
            moderationEngine_->processItem(aiContent);
            
            ChatMessage aiMsg;
            aiMsg.id = generateId();
            aiMsg.role = "assistant";
            aiMsg.content = aiResponse;
            aiMsg.timestamp = std::time(nullptr);
            aiMsg.was_blocked = false;
            
            // Check if AI response should be blocked (safety layer)
            if (aiContent.decision.auto_action == "block") {
                aiMsg.was_blocked = true;
                aiMsg.block_reason = "AI response flagged by safety systems";
                aiResponse = "I apologize, but I cannot provide that response as it was flagged by our safety systems.";
                aiMsg.content = aiResponse;
            }
            
            chatHistory_.push_back(aiMsg);
            
            json response = {
                {"id", aiMsg.id},
                {"message", aiResponse},
                {"blocked", aiMsg.was_blocked},
                {"timestamp", aiMsg.timestamp},
                {"moderation_score", {
                    {"user_message", {
                        {"sexual", userContent.moderation.labels.sexual},
                        {"violence", userContent.moderation.labels.violence},
                        {"hate", userContent.moderation.labels.hate}
                    }},
                    {"ai_response", {
                        {"sexual", aiContent.moderation.labels.sexual},
                        {"violence", aiContent.moderation.labels.violence},
                        {"hate", aiContent.moderation.labels.hate}
                    }}
                }}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error in chat: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Get chat history
    svr.Get("/api/chat/history", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            int limit = 50;
            if (req.has_param("limit")) {
                limit = std::stoi(req.get_param_value("limit"));
            }
            
            json response = json::array();
            int count = 0;
            
            for (auto it = chatHistory_.rbegin(); it != chatHistory_.rend() && count < limit; ++it, ++count) {
                response.push_back({
                    {"id", it->id},
                    {"role", it->role},
                    {"content", it->content},
                    {"timestamp", it->timestamp},
                    {"was_blocked", it->was_blocked},
                    {"block_reason", it->block_reason}
                });
            }
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error getting chat history: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Clear chat history
    svr.Delete("/api/chat/history", [this](const httplib::Request&, httplib::Response& res) {
        chatHistory_.clear();
        json response = {{"success", true}, {"message", "Chat history cleared"}};
        res.set_content(response.dump(), "application/json");
    });
    
    // ===== AI DETECTION & SOURCE IDENTIFICATION ENDPOINTS =====
    
    // Detect AI-generated content and identify source
    svr.Post("/api/detect/ai", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("text")) {
                json error = {{"error", "Missing required field: text"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            std::string text = body["text"].get<std::string>();
            
            ContentItem item;
            item.id = generateId();
            item.text_content = text;
            item.content_type = "text";
            item.source = "api";
            item.timestamp = std::time(nullptr);
            
            moderationEngine_->processItem(item);
            
            // Analyze patterns to identify potential AI source
            std::string detectedSource = "unknown";
            std::vector<std::string> indicators;
            
            // Simple heuristics (in production, use more sophisticated ML models)
            if (item.ai_detection.ai_score > 0.8) {
                if (text.find("As an AI") != std::string::npos || text.find("I'm Claude") != std::string::npos) {
                    detectedSource = "Claude (Anthropic)";
                    indicators.push_back("Contains Claude-specific phrasing");
                } else if (text.find("I'm ChatGPT") != std::string::npos || text.find("OpenAI") != std::string::npos) {
                    detectedSource = "ChatGPT (OpenAI)";
                    indicators.push_back("Contains ChatGPT-specific phrasing");
                } else if (text.find("I'm Gemini") != std::string::npos || text.find("Google AI") != std::string::npos) {
                    detectedSource = "Gemini (Google)";
                    indicators.push_back("Contains Gemini-specific phrasing");
                } else {
                    detectedSource = "Generic LLM";
                    indicators.push_back("High AI probability without specific markers");
                }
                
                // Additional indicators
                if (text.length() > 200 && text.find('\n') != std::string::npos) {
                    indicators.push_back("Well-structured with paragraphs");
                }
                if (std::count(text.begin(), text.end(), '.') > 3) {
                    indicators.push_back("Formal sentence structure");
                }
            }
            
            json response = {
                {"ai_score", item.ai_detection.ai_score},
                {"ai_confidence", item.ai_detection.confidence},
                {"is_ai_generated", item.ai_detection.label == "ai"},
                {"detected_source", detectedSource},
                {"indicators", indicators},
                {"model_used", item.ai_detection.model},
                {"analysis", {
                    {"text_length", text.length()},
                    {"sentence_count", std::count(text.begin(), text.end(), '.') + std::count(text.begin(), text.end(), '!') + std::count(text.begin(), text.end(), '?')},
                    {"avg_word_length", text.length() / (std::count(text.begin(), text.end(), ' ') + 1)}
                }}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error detecting AI: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // ===== REDDIT SCRAPER ENDPOINTS =====
    
    if (redditScraper_) {
        // Get scraper status
        svr.Get("/api/reddit/status", [this](const httplib::Request&, httplib::Response& res) {
            json response = {
                {"is_running", redditScraper_->isScraping()},
                {"subreddits", redditScraper_->getSubreddits()}
            };
            res.set_content(response.dump(), "application/json");
        });
        
        // Start scraping
        svr.Post("/api/reddit/start", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                
                if (!body.contains("subreddits")) {
                    json error = {{"error", "Missing required field: subreddits"}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }
                
                std::vector<std::string> subreddits = body["subreddits"].get<std::vector<std::string>>();
                int interval = body.value("interval", 300); // Default 5 minutes
                
                redditScraper_->setSubreddits(subreddits);
                
                // Set callback to process scraped items
                redditScraper_->setOnItemScraped([this](const ContentItem& item) {
                    ContentItem processedItem = item;
                    moderationEngine_->processItem(processedItem);
                    storage_->saveContent(processedItem);
                });
                
                redditScraper_->start(interval);
                
                json response = {
                    {"success", true},
                    {"message", "Reddit scraper started"},
                    {"subreddits", subreddits},
                    {"interval_seconds", interval}
                };
                res.set_content(response.dump(), "application/json");
            } catch (const std::exception& e) {
                Logger::error("Error starting Reddit scraper: " + std::string(e.what()));
                json error = {{"error", e.what()}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });
        
        // Stop scraping
        svr.Post("/api/reddit/stop", [this](const httplib::Request&, httplib::Response& res) {
            redditScraper_->stop();
            json response = {{"success", true}, {"message", "Reddit scraper stopped"}};
            res.set_content(response.dump(), "application/json");
        });
        
        // Scrape once (manual trigger)
        svr.Post("/api/reddit/scrape", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto body = json::parse(req.body);
                
                if (body.contains("subreddits")) {
                    std::vector<std::string> subreddits = body["subreddits"].get<std::vector<std::string>>();
                    redditScraper_->setSubreddits(subreddits);
                }
                
                auto items = redditScraper_->scrapeOnce();
                
                // Process all items
                for (auto& item : items) {
                    moderationEngine_->processItem(item);
                    storage_->saveContent(item);
                }
                
                json response = {
                    {"success", true},
                    {"items_scraped", items.size()},
                    {"items", json::array()}
                };
                
                for (const auto& item : items) {
                    response["items"].push_back(json::parse(item.toJson()));
                }
                
                res.set_content(response.dump(), "application/json");
            } catch (const std::exception& e) {
                Logger::error("Error scraping Reddit: " + std::string(e.what()));
                json error = {{"error", e.what()}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });
        
        // Get scraped items
        svr.Get("/api/reddit/items", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string subreddit = req.get_param_value("subreddit");
                
                auto items = storage_->loadAllContent();
                json response = json::array();
                
                for (const auto& item : items) {
                    if (item.source == "reddit") {
                        if (subreddit.empty() || item.subreddit == subreddit) {
                            response.push_back(json::parse(item.toJson()));
                        }
                    }
                }
                
                res.set_content(response.dump(), "application/json");
            } catch (const std::exception& e) {
                Logger::error("Error getting Reddit items: " + std::string(e.what()));
                json error = {{"error", e.what()}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });
    }
    
    // ===== STATISTICS ENDPOINTS =====
    
    // Get statistics
    svr.Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
        try {
            auto items = storage_->loadAllContent();
            
            int totalCount = items.size();
            int blockedCount = 0;
            int reviewCount = 0;
            int allowedCount = 0;
            int textCount = 0;
            int imageCount = 0;
            int aiGeneratedCount = 0;
            int redditCount = 0;
            
            double avgAiScore = 0.0;
            double avgSexualScore = 0.0;
            double avgViolenceScore = 0.0;
            
            std::map<std::string, int> subredditCounts;
            
            for (const auto& item : items) {
                if (item.decision.auto_action == "block") blockedCount++;
                else if (item.decision.auto_action == "review") reviewCount++;
                else if (item.decision.auto_action == "allow") allowedCount++;
                
                if (item.content_type == "text") textCount++;
                else if (item.content_type == "image") imageCount++;
                
                if (item.ai_detection.label == "ai") aiGeneratedCount++;
                
                if (item.source == "reddit") {
                    redditCount++;
                    if (!item.subreddit.empty()) {
                        subredditCounts[item.subreddit]++;
                    }
                }
                
                avgAiScore += item.ai_detection.ai_score;
                avgSexualScore += item.moderation.labels.sexual;
                avgViolenceScore += item.moderation.labels.violence;
            }
            
            if (totalCount > 0) {
                avgAiScore /= totalCount;
                avgSexualScore /= totalCount;
                avgViolenceScore /= totalCount;
            }
            
            json response = {
                {"total_items", totalCount},
                {"blocked", blockedCount},
                {"review", reviewCount},
                {"allowed", allowedCount},
                {"text_items", textCount},
                {"image_items", imageCount},
                {"ai_generated", aiGeneratedCount},
                {"reddit_items", redditCount},
                {"avg_ai_score", avgAiScore},
                {"avg_sexual_score", avgSexualScore},
                {"avg_violence_score", avgViolenceScore},
                {"subreddit_breakdown", subredditCounts},
                {"chat_messages", chatHistory_.size()}
            };
            
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            Logger::error("Error getting stats: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // ===== EXPORT ENDPOINTS =====
    
    // Export data
    svr.Get("/api/export", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string format = req.get_param_value("format");
            if (format.empty()) format = "json";
            
            auto items = storage_->loadAllContent();
            std::string filename = dataPath_ + "/exports/" + std::to_string(std::time(nullptr)) + "." + format;
            
            if (format == "csv") {
                Exporter::exportToCSV(items, filename);
                res.set_content("CSV export created: " + filename, "text/plain");
            } else if (format == "json") {
                Exporter::exportToJSON(items, filename);
                res.set_content("JSON export created: " + filename, "text/plain");
            } else {
                json error = {{"error", "Unsupported format. Use 'csv' or 'json'"}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            Logger::error("Error exporting data: " + std::string(e.what()));
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // Start server
    Logger::info("API server listening on http://0.0.0.0:" + std::to_string(port_));
    svr.listen("0.0.0.0", port_);
}

void ApiServer::stop() {
    Logger::info("Stopping API server");
}

} // namespace ModAI
