#pragma once

#include "network/HttpClient.h"
#include <string>
#include <map>

namespace ModAI {

class CurlHttpClient : public HttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    HttpResponse post(const HttpRequest& req) override;
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) override;

private:
    // Helper to perform the request
    HttpResponse perform(const std::string& url, const std::string& method, 
                         const std::map<std::string, std::string>& headers, 
                         const std::string& body);
};

} // namespace ModAI
