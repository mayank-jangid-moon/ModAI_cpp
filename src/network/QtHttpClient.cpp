#include "network/QtHttpClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QEventLoop>
#include <QTimer>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "utils/Logger.h"

namespace ModAI {

QtHttpClient::QtHttpClient(QObject* parent)
    : QObject(parent)
    , networkManager_(std::make_unique<QNetworkAccessManager>(this))
    , timeoutMs_(30000) {
}

QNetworkRequest QtHttpClient::createRequest(const std::string& url, const std::map<std::string, std::string>& headers) {
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key), QByteArray::fromStdString(value));
    }
    
    return request;
}

HttpResponse QtHttpClient::executeRequest(QNetworkReply* reply) {
    HttpResponse response;
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeoutMs_);
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start();
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll().toStdString();
        response.success = (reply->error() == QNetworkReply::NoError);
        
        if (!response.success) {
            response.errorMessage = reply->errorString().toStdString();
        }
        
        // Read headers
        auto headers = reply->rawHeaderPairs();
        for (const auto& header : headers) {
            response.headers[header.first.toStdString()] = header.second.toStdString();
        }
    } else {
        // Timeout
        response.success = false;
        response.errorMessage = "Request timeout";
        reply->abort();
    }
    
    reply->deleteLater();
    return response;
}

HttpResponse QtHttpClient::post(const HttpRequest& req) {
    HttpResponse response;
    int retries = 0;
    
    while (retries <= maxRetries_) {
        QNetworkRequest request = createRequest(req.url, req.headers);
        QNetworkReply* reply = nullptr;
        
        if (req.contentType == "multipart/form-data" && !req.binaryData.empty()) {
            QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            
            QHttpPart filePart;
            filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                              QVariant("form-data; name=\"file\"; filename=\"image.jpg\""));
            filePart.setBody(QByteArray(reinterpret_cast<const char*>(req.binaryData.data()), 
                                       req.binaryData.size()));
            
            multiPart->append(filePart);
            
            request.setRawHeader("Content-Type", "multipart/form-data; boundary=" + multiPart->boundary());
            reply = networkManager_->post(request, multiPart);
            multiPart->setParent(reply);
        } else {
            if (!req.contentType.empty()) {
                request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray::fromStdString(req.contentType));
            } else {
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            }
            
            QByteArray data;
            if (!req.body.empty()) {
                data = QByteArray::fromStdString(req.body);
            } else if (!req.binaryData.empty()) {
                data = QByteArray(reinterpret_cast<const char*>(req.binaryData.data()), req.binaryData.size());
            }
            
            reply = networkManager_->post(request, data);
        }
        
        response = executeRequest(reply);
        
        // Success - return immediately
        if (response.success) {
            return response;
        }
        
        // Permanent client errors (except 429) - don't retry
        if (response.statusCode >= 400 && response.statusCode < 500 && response.statusCode != 429) {
            return response;
        }
        
        // Only retry on rate limits (429) or server errors (5xx)
        if (response.statusCode == 429 || response.statusCode >= 500) {
            retries++;
            if (retries <= maxRetries_) {
                int delay = retryDelayMs_ * (1 << (retries - 1));
                Logger::warn("Request failed (" + std::to_string(response.statusCode) + "), retrying in " + std::to_string(delay) + "ms. Attempt " + std::to_string(retries));
                
                QEventLoop loop;
                QTimer::singleShot(delay, &loop, &QEventLoop::quit);
                loop.exec();
            } else {
                return response;
            }
        } else {
            return response;
        }
    }
    return response;
}

HttpResponse QtHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    int retries = 0;
    
    while (retries <= maxRetries_) {
        QNetworkRequest request = createRequest(url, headers);
        QNetworkReply* reply = networkManager_->get(request);
        
        response = executeRequest(reply);
        
        // Success - return immediately
        if (response.success) {
            return response;
        }
        
        // Permanent client errors (except 429) - don't retry
        if (response.statusCode >= 400 && response.statusCode < 500 && response.statusCode != 429) {
            return response;
        }
        
        // Only retry on rate limits (429) or server errors (5xx)
        if (response.statusCode == 429 || response.statusCode >= 500) {
            retries++;
            if (retries <= maxRetries_) {
                int delay = retryDelayMs_ * (1 << (retries - 1));
                Logger::warn("Request failed (" + std::to_string(response.statusCode) + "), retrying in " + std::to_string(delay) + "ms. Attempt " + std::to_string(retries));
                
                QEventLoop loop;
                QTimer::singleShot(delay, &loop, &QEventLoop::quit);
                loop.exec();
            } else {
                return response;
            }
        } else {
            return response;
        }
    }
    return response;
}

} // namespace ModAI

