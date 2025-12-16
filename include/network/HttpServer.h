#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include "core/AppController.h"
#include <nlohmann/json.hpp>

namespace ModAI {

class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(AppController* controller, int port = 8080, QObject* parent = nullptr);
    void start();

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QTcpServer* server_;
    AppController* controller_;
    int port_;
    
    void handleRequest(QTcpSocket* socket);
    void sendResponse(QTcpSocket* socket, int statusCode, const QByteArray& body, const QString& contentType = "application/json");
    void sendJson(QTcpSocket* socket, const nlohmann::json& json);
    void sendError(QTcpSocket* socket, int statusCode, const std::string& message);
    
    // Helpers
    struct Request {
        QString method;
        QString path;
        std::map<QString, QString> queryParams;
        QByteArray body;
    };
    
    Request parseRequest(QTcpSocket* socket);
};

}
