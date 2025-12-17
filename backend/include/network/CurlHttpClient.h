#pragma once

#include "network/HttpClient.h"
#include <memory>
#include <curl/curl.h>

namespace ModAI {

class CurlHttpClient : public HttpClient {
private:
    int timeoutMs_;
    int maxRetries_;
    int retryDelayMs_;
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    
    HttpResponse executeRequest(CURL* curl, const HttpRequest& req, struct curl_slist* headers);
    
public:
    CurlHttpClient();
    virtual ~CurlHttpClient();
    
    HttpResponse post(const HttpRequest& req) override;
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {}) override;
    
    void setTimeout(int milliseconds) { timeoutMs_ = milliseconds; }
    void setRetries(int maxRetries, int delayMs) { maxRetries_ = maxRetries; retryDelayMs_ = delayMs; }
};

} // namespace ModAI
