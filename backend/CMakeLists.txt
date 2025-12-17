cmake_minimum_required(VERSION 3.20)
project(ModAI_Backend VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Find ONNX Runtime for local AI inference (optional)
find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
        PATHS /usr/include /usr/local/include
        PATH_SUFFIXES onnxruntime/core/session)
    find_library(ONNXRUNTIME_LIBRARY NAMES onnxruntime
        PATHS /usr/lib /usr/local/lib)
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        set(ONNXRUNTIME_FOUND TRUE)
        message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
    endif()
endif()

# Include cpp-httplib (header-only library)
# You can either install it system-wide or add it as a submodule/vendored
find_path(HTTPLIB_INCLUDE_DIR httplib.h
    PATHS /usr/include /usr/local/include
    PATH_SUFFIXES cpp-httplib httplib)

if(NOT HTTPLIB_INCLUDE_DIR)
    message(STATUS "cpp-httplib not found, will download...")
    include(FetchContent)
    FetchContent_Declare(
        cpp-httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.14.3
    )
    FetchContent_MakeAvailable(cpp-httplib)
    set(HTTPLIB_INCLUDE_DIR ${cpp-httplib_SOURCE_DIR})
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${HTTPLIB_INCLUDE_DIR}
)

# Source files
set(HEADERS
    include/core/ContentItem.h
    include/core/ModerationEngine.h
    include/core/RuleEngine.h
    include/core/ResultCache.h
    include/detectors/TextDetector.h
    include/detectors/ImageModerator.h
    include/detectors/TextModerator.h
    include/detectors/LocalAIDetector.h
    include/detectors/HiveImageModerator.h
    include/detectors/HiveTextModerator.h
    include/network/HttpClient.h
    include/network/CurlHttpClient.h
    include/network/RateLimiter.h
    include/storage/Storage.h
    include/storage/JsonlStorage.h
    include/export/Exporter.h
    include/utils/Logger.h
    include/utils/Crypto.h
    include/api/ApiServer.h
    include/scraper/RedditScraper.h
)

set(SOURCES
    src/main.cpp
    src/core/ContentItem.cpp
    src/core/ModerationEngine.cpp
    src/core/RuleEngine.cpp
    src/core/ResultCache.cpp
    src/detectors/LocalAIDetector.cpp
    src/detectors/HiveImageModerator.cpp
    src/detectors/HiveTextModerator.cpp
    src/network/CurlHttpClient.cpp
    src/network/RateLimiter.cpp
    src/storage/JsonlStorage.cpp
    src/export/Exporter.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
    src/api/ApiServer.cpp
    src/scraper/RedditScraper.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    CURL::libcurl
    pthread
)

# Optional: link ONNX Runtime if found
if(ONNXRUNTIME_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PRIVATE ONNXRUNTIME_FOUND)
    message(STATUS "Local AI detection enabled with ONNX Runtime")
else()
    message(STATUS "ONNX Runtime not found - local AI detection disabled")
    message(STATUS "Install ONNX Runtime for local inference support")
endif()

# Platform-specific settings
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
