cmake_minimum_required(VERSION 3.20)
project(ModAI_Core VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
# find_package(nlohmann_json 3.2.0 REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/backend/include)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(onnxruntime QUIET)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Core Sources (Qt-Free)
set(CORE_SOURCES
    src/core/ModerationEngine.cpp
    src/core/RuleEngine.cpp
    src/core/ResultCache.cpp
    src/core/ContentItem.cpp
    src/core/CoreController.cpp
    src/network/CurlHttpClient.cpp
    src/network/RateLimiter.cpp
    src/detectors/LocalAIDetector.cpp
    src/detectors/HiveImageModerator.cpp
    src/detectors/HiveTextModerator.cpp
    src/scraper/RedditScraper.cpp
    src/storage/JsonlStorage.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
)

# Server Executable
add_executable(modai-server src/server_main.cpp ${CORE_SOURCES})

target_link_libraries(modai-server PRIVATE
    CURL::libcurl
    OpenSSL::Crypto
    pthread
)

if(ONNXRUNTIME_FOUND)
    target_link_libraries(modai-server PRIVATE ${ONNXRUNTIME_LIBRARY})
    target_compile_definitions(modai-server PRIVATE ONNXRUNTIME_FOUND)
endif()

# Legacy Qt App (Optional, if needed)
# add_subdirectory(qt_app) 
