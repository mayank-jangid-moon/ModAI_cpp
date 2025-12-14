#pragma once

#include "network/HttpClient.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>

namespace ModAI {

class QtHttpClient : public QObject, public HttpClient {
    Q_OBJECT

private:
    std::unique_ptr<QNetworkAccessManager> networkManager_;
    int timeoutMs_;

public:
    explicit QtHttpClient(QObject* parent = nullptr);
    ~QtHttpClient() override = default;
    
    HttpResponse post(const HttpRequest& req) override;
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) override;
    
    void setTimeout(int milliseconds) { timeoutMs_ = milliseconds; }

private:
    HttpResponse executeRequest(QNetworkReply* reply);
    QNetworkRequest createRequest(const std::string& url, const std::map<std::string, std::string>& headers);
};

} // namespace ModAI

