#pragma once

#include <string>
#include <functional>
#include <map>

namespace ModAI {

struct ServerRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> queryParams;
    std::string body;
};

struct ServerResponse {
    int statusCode = 200;
    std::string body;
    std::string contentType = "application/json";
};

class HttpServerInterface {
public:
    virtual ~HttpServerInterface() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    
    using Handler = std::function<ServerResponse(const ServerRequest&)>;
    virtual void registerHandler(const std::string& method, const std::string& path, Handler handler) = 0;
};

} // namespace ModAI
