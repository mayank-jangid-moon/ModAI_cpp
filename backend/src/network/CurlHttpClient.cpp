#include "network/CurlHttpClient.h"
#include "utils/Logger.h"
#include <thread>
#include <chrono>
#include <sstream>

namespace ModAI {

static bool curlGlobalInit = false;

CurlHttpClient::CurlHttpClient()
    : timeoutMs_(30000)
    , maxRetries_(3)
    , retryDelayMs_(1000) {
    
    if (!curlGlobalInit) {
        curl_global_init(CURL_GLOBAL_ALL);
        curlGlobalInit = true;
    }
}

CurlHttpClient::~CurlHttpClient() {
    // Note: curl_global_cleanup() should only be called once at program exit
}

size_t CurlHttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t CurlHttpClient::headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalSize = size * nitems;
    std::map<std::string, std::string>* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    
    std::string header(buffer, totalSize);
    size_t colonPos = header.find(':');
    if (colonPos != std::string::npos) {
        std::string key = header.substr(0, colonPos);
        std::string value = header.substr(colonPos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        (*headers)[key] = value;
    }
    
    return totalSize;
}

HttpResponse CurlHttpClient::executeRequest(CURL* curl, const HttpRequest& req, struct curl_slist* headers) {
    HttpResponse response;
    std::string responseBody;
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs_);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Follow redirects (needed for Reddit and other sites)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);
        response.body = responseBody;
        response.success = (httpCode >= 200 && httpCode < 300);
        
        if (!response.success) {
            Logger::debug("HTTP error - status: " + std::to_string(response.statusCode) +
                         ", Body length: " + std::to_string(response.body.length()));
        }
    } else {
        response.success = false;
        response.errorMessage = curl_easy_strerror(res);
        Logger::error("CURL error: " + response.errorMessage);
    }
    
    return response;
}

HttpResponse CurlHttpClient::post(const HttpRequest& req) {
    HttpResponse response;
    int retries = 0;
    
    while (retries <= maxRetries_) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            response.success = false;
            response.errorMessage = "Failed to initialize CURL";
            return response;
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        
        struct curl_slist* headersList = nullptr;
        
        // Add headers
        for (const auto& [key, value] : req.headers) {
            std::string header = key + ": " + value;
            headersList = curl_slist_append(headersList, header.c_str());
        }
        
        // Handle different content types
        if (req.contentType == "multipart/form-data" && !req.binaryData.empty()) {
            curl_mime* mime = curl_mime_init(curl);
            curl_mimepart* part = curl_mime_addpart(mime);
            
            curl_mime_name(part, "file");
            curl_mime_data(part, reinterpret_cast<const char*>(req.binaryData.data()), req.binaryData.size());
            curl_mime_filename(part, "image.jpg");
            curl_mime_type(part, "application/octet-stream");
            
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        } else {
            // Set content type if not already in headers
            if (req.contentType.empty() && req.headers.find("Content-Type") == req.headers.end()) {
                headersList = curl_slist_append(headersList, "Content-Type: application/json");
            } else if (!req.contentType.empty()) {
                std::string ctHeader = "Content-Type: " + req.contentType;
                headersList = curl_slist_append(headersList, ctHeader.c_str());
            }
            
            if (!req.body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.body.length());
            } else if (!req.binaryData.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.binaryData.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.binaryData.size());
            }
        }
        
        response = executeRequest(curl, req, headersList);
        
        curl_slist_free_all(headersList);
        curl_easy_cleanup(curl);
        
        // Success - return immediately
        if (response.success) {
            return response;
        }
        
        // Permanent client errors (except 429) - don't retry
        if (response.statusCode >= 400 && response.statusCode < 500 && response.statusCode != 429) {
            return response;
        }
        
        // Only retry on rate limits (429) or server errors (5xx) or network errors
        if (response.statusCode == 429 || response.statusCode >= 500 || response.statusCode == 0) {
            retries++;
            if (retries <= maxRetries_) {
                int delay = retryDelayMs_ * (1 << (retries - 1));
                Logger::warn("Request failed (" + std::to_string(response.statusCode) + 
                           "), retrying in " + std::to_string(delay) + "ms. Attempt " + 
                           std::to_string(retries));
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                continue;
            }
        }
        
        break;
    }
    
    return response;
}

HttpResponse CurlHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        HttpResponse response;
        response.success = false;
        response.errorMessage = "Failed to initialize CURL";
        return response;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    
    struct curl_slist* headersList = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        headersList = curl_slist_append(headersList, header.c_str());
    }
    
    HttpRequest dummyReq;  // Just for passing to executeRequest
    HttpResponse response = executeRequest(curl, dummyReq, headersList);
    
    curl_slist_free_all(headersList);
    curl_easy_cleanup(curl);
    
    return response;
}

} // namespace ModAI
