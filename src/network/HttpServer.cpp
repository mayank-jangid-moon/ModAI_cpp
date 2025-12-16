#include "network/HttpServer.h"
#include "utils/Logger.h"
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

namespace ModAI {

HttpServer::HttpServer(AppController* controller, int port, QObject* parent)
    : QObject(parent), controller_(controller), port_(port) {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
}

void HttpServer::start() {
    if (server_->listen(QHostAddress::Any, port_)) {
        Logger::info("HTTP Server started on port " + std::to_string(port_));
    } else {
        Logger::error("Failed to start HTTP Server: " + server_->errorString().toStdString());
    }
}

void HttpServer::onNewConnection() {
    QTcpSocket* socket = server_->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void HttpServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    // Simple check to ensure we have the full header (not robust for production but ok for local)
    if (socket->canReadLine()) {
        handleRequest(socket);
    }
}

HttpServer::Request HttpServer::parseRequest(QTcpSocket* socket) {
    Request req;
    
    // Read Request Line
    QByteArray line = socket->readLine();
    QStringList parts = QString(line).trimmed().split(' ');
    if (parts.size() >= 2) {
        req.method = parts[0];
        QString fullPath = parts[1];
        QUrl url(fullPath);
        req.path = url.path();
        
        QUrlQuery query(url);
        for (auto item : query.queryItems()) {
            req.queryParams[item.first] = item.second;
        }
    }
    
    // Read Headers (skip for now, just read until empty line)
    int contentLength = 0;
    while (socket->canReadLine()) {
        QByteArray headerLine = socket->readLine();
        if (headerLine.trimmed().isEmpty()) break;
        
        if (headerLine.startsWith("Content-Length:")) {
            contentLength = headerLine.mid(15).trimmed().toInt();
        }
    }
    
    // Read Body
    if (contentLength > 0) {
        req.body = socket->read(contentLength);
    }
    
    return req;
}

void HttpServer::handleRequest(QTcpSocket* socket) {
    Request req = parseRequest(socket);
    Logger::info("Request: " + req.method.toStdString() + " " + req.path.toStdString());
    
    if (req.method == "OPTIONS") {
        QString response = "HTTP/1.1 200 OK\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Content-Type\r\n"
                           "\r\n";
        socket->write(response.toUtf8());
        socket->disconnectFromHost();
        return;
    }

    try {
        if (req.path == "/api/health" && req.method == "GET") {
            nlohmann::json resp;
            resp["status"] = "ok";
            resp["version"] = "1.0.0";
            sendJson(socket, resp);
        }
        else if (req.path == "/api/status" && req.method == "GET") {
            auto status = controller_->getStatus();
            nlohmann::json resp;
            resp["scraping_active"] = status.scraping_active;
            resp["current_subreddit"] = status.current_subreddit;
            resp["queue_size"] = status.queue_size;
            resp["items_processed"] = status.items_processed;
            sendJson(socket, resp);
        }
        else if (req.path == "/api/scraper/start" && req.method == "POST") {
            auto json = nlohmann::json::parse(req.body.toStdString());
            std::string subreddit = json.value("subreddit", "");
            if (subreddit.empty()) {
                sendError(socket, 400, "Missing subreddit");
                return;
            }
            controller_->startScraping(subreddit);
            nlohmann::json resp;
            resp["status"] = "started";
            resp["subreddit"] = subreddit;
            sendJson(socket, resp);
        }
        else if (req.path == "/api/scraper/stop" && req.method == "POST") {
            controller_->stopScraping();
            nlohmann::json resp;
            resp["status"] = "stopped";
            sendJson(socket, resp);
        }
        else if (req.path == "/api/items" && req.method == "GET") {
            int page = req.queryParams.count("page") ? req.queryParams["page"].toInt() : 1;
            int limit = req.queryParams.count("limit") ? req.queryParams["limit"].toInt() : 50;
            std::string status = req.queryParams.count("status") ? req.queryParams["status"].toStdString() : "all";
            std::string search = req.queryParams.count("search") ? req.queryParams["search"].toStdString() : "";
            
            auto items = controller_->getItems(page, limit, status, search);
            
            nlohmann::json data = nlohmann::json::array();
            for (const auto& item : items) {
                data.push_back(nlohmann::json::parse(item.toJson()));
            }
            
            nlohmann::json resp;
            resp["data"] = data;
            resp["pagination"]["current_page"] = page;
            // Total pages calculation omitted for brevity, would need total count from controller
            sendJson(socket, resp);
        }
        else if (req.path.startsWith("/api/items/") && req.method == "GET") {
            // Extract ID
            QString id = req.path.mid(11); // /api/items/
            auto item = controller_->getItem(id.toStdString());
            if (item) {
                sendJson(socket, nlohmann::json::parse(item->toJson()));
            } else {
                sendError(socket, 404, "Item not found");
            }
        }
        else if (req.path.startsWith("/api/items/") && req.path.endsWith("/decision") && req.method == "POST") {
             // Extract ID: /api/items/{id}/decision
             QString idPart = req.path.mid(11);
             QString id = idPart.left(idPart.length() - 9); // remove /decision
             
             auto json = nlohmann::json::parse(req.body.toStdString());
             std::string action = json.value("action", "");
             std::string reason = json.value("reason", "");
             
             controller_->submitAction(id.toStdString(), action, reason);
             
             nlohmann::json resp;
             resp["id"] = id.toStdString();
             resp["status"] = "updated";
             resp["new_decision"] = action;
             sendJson(socket, resp);
        }
        else if (req.path == "/api/stats" && req.method == "GET") {
            auto stats = controller_->getStats();
            nlohmann::json resp;
            resp["total_scanned"] = stats.total_scanned;
            resp["flagged_count"] = stats.flagged_count;
            resp["action_needed"] = stats.action_needed;
            resp["ai_generated_count"] = stats.ai_generated_count;
            sendJson(socket, resp);
        }
        else {
            sendError(socket, 404, "Not Found");
        }
    } catch (const std::exception& e) {
        sendError(socket, 500, e.what());
    }
}

void HttpServer::sendResponse(QTcpSocket* socket, int statusCode, const QByteArray& body, const QString& contentType) {
    // Note: Status line already sent in handleRequest for CORS hack. 
    // But properly we should buffer and send all at once.
    // For this simple implementation, I already sent "HTTP/1.1 200 OK" in handleRequest.
    // This is a bit messy. Let's fix it.
    // I will remove the early write in handleRequest and do it here properly.
}

void HttpServer::sendJson(QTcpSocket* socket, const nlohmann::json& json) {
    std::string body = json.dump();
    QString response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + QString::number(body.length()) + "\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Access-Control-Allow-Headers: Content-Type\r\n"
                       "\r\n" + QString::fromStdString(body);
    
    socket->write(response.toUtf8());
    socket->disconnectFromHost();
}

void HttpServer::sendError(QTcpSocket* socket, int statusCode, const std::string& message) {
    nlohmann::json json;
    json["error"] = message;
    std::string body = json.dump();
    
    QString statusStr = "200 OK"; // Keeping 200 for simplicity with CORS, but ideally should be real status
    if (statusCode == 400) statusStr = "400 Bad Request";
    if (statusCode == 404) statusStr = "404 Not Found";
    if (statusCode == 500) statusStr = "500 Internal Server Error";
    
    QString response = "HTTP/1.1 " + statusStr + "\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + QString::number(body.length()) + "\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "\r\n" + QString::fromStdString(body);
                       
    socket->write(response.toUtf8());
    socket->disconnectFromHost();
}

}
