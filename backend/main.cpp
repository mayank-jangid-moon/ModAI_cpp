#include <iostream>
#include <string>
#include <curl/curl.h>

size_t write_callback(void* content, size_t size, size_t num, void* userdata){
    size_t total_size = size * num;
    std::string* response = static_cast<std::string*>(userdata);
    response->append(static_cast<char*>(content), total_size);
    // std::cout << "callback called" << std::endl;
    
    return total_size;
}

int main(){

    CURL* curl;
    CURLcode res;
    std::string response;
    
    std::string data = "{\n\"contents\": [\n{\n\"parts\": [\n{\n\"text\": \"Explain how AI works in a few words\"\n}\n]\n}\n]\n}";
    struct curl_slist* http_header = NULL;
    http_header = curl_slist_append(http_header, "x-goog-api-key: AIzaSyDaVc1CIQxMVGyLZiI8WMc7rmEMqwHQLEc");
    http_header = curl_slist_append(http_header, "Content-Type: application/json");


    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-pro:generateContent";

    curl = curl_easy_init();
    if(!curl) std::cerr << "ERROR: curl_easy_init() failed" << std::endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    
    res = curl_easy_perform(curl);
    if(res != CURLE_OK) std::cerr << "ERROR: curl_easy_perform() failed -> " << curl_easy_strerror(res) << std::endl;

    std::cout << response << std::endl;
    return 0;
}