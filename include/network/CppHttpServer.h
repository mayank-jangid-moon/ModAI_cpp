#pragma once

#include "core/interfaces/HttpServerInterface.h"
#include "httplib.h"
#include "utils/Logger.h"

namespace ModAI {

class CppHttpServer : public HttpServerInterface {
private:
    httplib::Server svr_;
    int port_;
    std::thread serverThread_;

public:
    explicit CppHttpServer(int port = 8080) : port_(port) {}
    
    ~CppHttpServer() {
        stop();
    }

    void start() override {
        serverThread_ = std::thread([this]() {
            Logger::info("Starting HTTP Server on port " + std::to_string(port_));
            svr_.listen("0.0.0.0", port_);
        });
    }

    void stop() override {
        if (svr_.is_running()) {
            svr_.stop();
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    void registerHandler(const std::string& method, const std::string& path, Handler handler) override {
        auto wrapper = [handler](const httplib::Request& req, httplib::Response& res) {
            ServerRequest sReq;
            sReq.method = req.method;
            sReq.path = req.path;
            sReq.body = req.body;
            for (const auto& p : req.params) {
                sReq.queryParams[p.first] = p.second;
            }

            ServerResponse sRes = handler(sReq);
            
            res.status = sRes.statusCode;
            res.set_content(sRes.body, sRes.contentType.c_str());
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        };

        if (method == "GET") svr_.Get(path.c_str(), wrapper);
        else if (method == "POST") svr_.Post(path.c_str(), wrapper);
        else if (method == "OPTIONS") svr_.Options(path.c_str(), [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 200;
        });
    }
};

} // namespace ModAI
