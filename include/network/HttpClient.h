#pragma once

#include <string>
#include <map>
#include <vector>

namespace ModAI {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success = false;
    std::string errorMessage;
};

struct HttpRequest {
    std::string url;
    std::string method;  // "GET", "POST", etc.
    std::map<std::string, std::string> headers;
    std::string body;
    std::vector<uint8_t> binaryData;  // For multipart/form-data
    std::string contentType;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    
    virtual HttpResponse post(const HttpRequest& req) = 0;
    virtual HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) = 0;
};

} // namespace ModAI

