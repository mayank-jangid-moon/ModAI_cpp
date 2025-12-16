#include "network/CurlHttpClient.h"
#include "utils/Logger.h"
#include <curl/curl.h>
#include <sstream>

namespace ModAI {

// Callback for writing received data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

CurlHttpClient::CurlHttpClient() {
    curl_global_init(CURL_GLOBAL_ALL);
}

CurlHttpClient::~CurlHttpClient() {
    curl_global_cleanup();
}

HttpResponse CurlHttpClient::post(const HttpRequest& req) {
    return perform(req.url, "POST", req.headers, req.body);
}

HttpResponse CurlHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    return perform(url, "GET", headers, "");
}

HttpResponse CurlHttpClient::perform(const std::string& url, const std::string& method, 
                                     const std::map<std::string, std::string>& headers, 
                                     const std::string& body) {
    HttpResponse response;
    CURL* curl = curl_easy_init();
    
    if (!curl) {
        response.success = false;
        response.errorMessage = "Failed to initialize CURL";
        return response;
    }

    std::string readBuffer;
    struct curl_slist* chunk = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30s timeout

    // Headers
    for (const auto& pair : headers) {
        std::string header = pair.first + ": " + pair.second;
        chunk = curl_slist_append(chunk, header.c_str());
    }
    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    // Method specific
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
    }

    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        response.success = false;
        response.errorMessage = curl_easy_strerror(res);
    } else {
        response.success = true;
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.statusCode = static_cast<int>(http_code);
        response.body = readBuffer;
    }

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    
    return response;
}

} // namespace ModAI
