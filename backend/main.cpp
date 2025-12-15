#include <iostream>
#include <string>
#include <curl/curl.h>
#include "include/nlohmann/json.hpp"
#include <stdexcept>

using json = nlohmann::json;

class CURLclass{
private:
    CURL* curl;
    CURLcode res;
    std::string url;
    std::string response_string;
    json response_json;
    std::string request_string;
    size_t (*write_callback)(void*, size_t, size_t, void*);
    struct curl_slist* http_header;

public:
    CURLclass(){
        curl = curl_easy_init();
        if(!curl) throw std::runtime_error("ERROR: curl_easy_init() malfunctioned.");
    }

    CURLclass(const std::string& url, size_t (*write_callback)(void*, size_t, size_t, void*), const std::string& request_string){
        this->url = url;
        this->write_callback = write_callback;
        this->request_string = request_string;
        http_header = NULL;
    }

    void init(){
        curl = curl_easy_init();
        if(!curl) throw std::runtime_error("ERROR: curl_easy_init() malfunctioned.");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_string.c_str()); 
    }

    void add_http_header(const std::string& header){
        http_header = curl_slist_append(http_header, header.c_str());
    }

    void response(){
        response_json = json::parse(response_string);
        std::cout << response_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>() << std::endl;
    }
    CURLcode perform(){
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) throw std::runtime_error("ERROR: curl_easy_perform() failed -> " + static_cast<std::string>(curl_easy_strerror(res)));
        json response_json = json::parse(response_string);
        return res;
    }

    ~CURLclass(){
        curl_easy_cleanup(curl);
    }
};

size_t write_callback(void* content, size_t size, size_t num, void* userdata){
    size_t total_size = size * num;
    std::string* response = static_cast<std::string*>(userdata);
    response->append(static_cast<char*>(content), total_size);
    
    return total_size;
}

int main(){
    std::string prompt;
    std::cout << "Enter the Prompt: ";
    std::getline(std::cin, prompt);
    
    json request_json = {
        {"contents", json::array({
            {
                {"parts", json::array({
                    {
                        {"text", prompt}
                    }
                })}
            }
        })}
    };

    std::string request_string = request_json.dump(2);

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";

    try {
        CURLclass api(url, &write_callback, request_string);
        api.add_http_header("x-goog-api-key: AIzaSyDaVc1CIQxMVGyLZiI8WMc7rmEMqwHQLEc");
        api.add_http_header("Content-Type: application/json");
        api.init();
        api.perform();
        api.response();
    }

    catch(std::runtime_error& e){
        std::cerr << "RUNTIME ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}