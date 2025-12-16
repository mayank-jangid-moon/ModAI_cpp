#pragma once

#include "network/HttpClient.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <memory>

namespace ModAI {

class QtHttpClient : public QObject, public HttpClient {
  Q_OBJECT

private:
  std::unique_ptr<QNetworkAccessManager> networkManager_;
  int timeoutMs_;
  int maxRetries_ = 3;
  int retryDelayMs_ = 1000;

public:
  explicit QtHttpClient(QObject *parent = nullptr);
  ~QtHttpClient() override = default;

  HttpResponse post(const HttpRequest &req) override;
  HttpResponse
  get(const std::string &url,
      const std::map<std::string, std::string> &headers = {}) override;

  void setTimeout(int milliseconds) { timeoutMs_ = milliseconds; }
  void setRetries(int maxRetries, int delayMs) {
    maxRetries_ = maxRetries;
    retryDelayMs_ = delayMs;
  }

private:
  HttpResponse executeRequest(QNetworkReply *reply);
  QNetworkRequest
  createRequest(const std::string &url,
                const std::map<std::string, std::string> &headers);
};

} // namespace ModAI
