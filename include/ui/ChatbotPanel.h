#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <memory>
#include "network/HttpClient.h"
#include "detectors/HiveTextModerator.h"

namespace ModAI {

/**
 * @brief Chat panel with LLM integration and Hive API railguard
 * 
 * This panel provides a chatbot interface where:
 * - User sends messages to an LLM (via API)
 * - LLM responses are analyzed by Hive API
 * - Offensive/inappropriate responses are blocked
 */
class ChatbotPanel : public QWidget {
    Q_OBJECT

public:
    explicit ChatbotPanel(QWidget* parent = nullptr);
    ~ChatbotPanel() = default;

    /**
     * @brief Initialize with HTTP client and Hive moderator
     */
    void initialize(std::unique_ptr<HttpClient> httpClient,
                   std::unique_ptr<HiveTextModerator> textModerator,
                   const std::string& llmApiKey = "",
                   const std::string& llmEndpoint = "");

signals:
    void messageBlocked(const QString& reason);
    void moderationResult(const QString& message, bool passed);

private slots:
    void onSendMessage();
    void onClearChat();

private:
    void setupUI();
    void addMessageToChat(const QString& role, const QString& message, bool blocked = false);
    void callLLM(const QString& userMessage);
    bool moderateResponse(const std::string& response);
    void showTypingIndicator();
    void hideTypingIndicator();
    void scrollToBottom();
    QWidget* createMessageBubble(const QString& message, bool isUser, const QString& timestamp, bool blocked = false);
    QString addWatermark(const QString& text, const QString& modelId);

    QScrollArea* chatScrollArea_;
    QWidget* chatContainer_;
    QVBoxLayout* chatLayout_;
    QLineEdit* messageInput_;
    QPushButton* sendButton_;
    QPushButton* clearButton_;
    QComboBox* modelSelector_;
    QLabel* statusLabel_;
    QFrame* typingIndicator_;
    
    bool isDarkTheme_{false};

public:
    void setTheme(bool isDark);
    
    /**
     * @brief Extract watermark from text if present
     * @return Model identifier or empty string if no watermark found
     */
    static QString extractWatermark(const QString& text);

    std::unique_ptr<HttpClient> httpClient_;
    std::unique_ptr<HiveTextModerator> textModerator_;
    std::string llmApiKey_;
    std::string llmEndpoint_;
    
    // Conversation history for context
    std::vector<std::pair<std::string, std::string>> conversationHistory_;
};

} // namespace ModAI
