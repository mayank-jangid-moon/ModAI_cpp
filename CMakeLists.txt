cmake_minimum_required(VERSION 3.20)
project(ModAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Network 
    Charts
    Concurrent
)

# Find nlohmann/json
find_package(nlohmann_json 3.2.0 REQUIRED)

# Optional: spdlog for logging
find_package(spdlog QUIET)
find_package(OpenSSL REQUIRED)

# Find ONNX Runtime for local AI inference
find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND)
    # Try to find it manually
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

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)


set(HEADERS
    include/core/ModerationEngine.h
    include/core/RuleEngine.h
    include/core/ContentItem.h
    include/core/ResultCache.h
    include/network/HttpClient.h
    include/network/QtHttpClient.h
    include/network/RateLimiter.h
    include/detectors/TextDetector.h
    include/detectors/LocalAIDetector.h
    include/detectors/ImageModerator.h
    include/detectors/HiveImageModerator.h
    include/detectors/TextModerator.h
    include/detectors/HiveTextModerator.h
    include/scraper/RedditScraper.h
    include/storage/Storage.h
    include/storage/JsonlStorage.h
    include/export/Exporter.h
    include/utils/Logger.h
    include/utils/Crypto.h
    include/ui/MainWindow.h
    include/ui/DashboardModel.h
    include/ui/DashboardProxyModel.h
    include/ui/DetailPanel.h
    include/ui/RailguardOverlay.h
    include/ui/ReviewDialog.h
)

set(SOURCES
    src/main.cpp
    src/core/ModerationEngine.cpp
    src/core/RuleEngine.cpp
    src/core/ResultCache.cpp
    src/core/ContentItem.cpp
    src/network/QtHttpClient.cpp
    src/network/RateLimiter.cpp
    src/detectors/LocalAIDetector.cpp
    src/detectors/HiveImageModerator.cpp
    src/detectors/HiveTextModerator.cpp
    src/scraper/RedditScraper.cpp
    src/storage/JsonlStorage.cpp
    src/export/Exporter.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
    src/ui/MainWindow.cpp
    src/ui/DashboardProxyModel.cpp
    src/ui/DashboardModel.cpp
    src/ui/DetailPanel.cpp
    src/ui/RailguardOverlay.cpp
    src/ui/ReviewDialog.cpp
)

# Enable Qt MOC BEFORE creating the executable
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    Qt6::Concurrent
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
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

# Optional: link spdlog if found
if(spdlog_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SPDLOG)
endif()

# Platform-specific settings
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Build test executable for ONNX inference
if(ONNXRUNTIME_FOUND)
    add_executable(test_onnx_inference tests/test_onnx_inference.cpp)
    target_include_directories(test_onnx_inference PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(test_onnx_inference PRIVATE ${ONNXRUNTIME_LIBRARY})
    target_compile_definitions(test_onnx_inference PRIVATE ONNXRUNTIME_FOUND)
    message(STATUS "Building ONNX inference test executable")
endif()

# Create data directory structure
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/cache)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/exports/reports)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/exports/csv)

# Tests (optional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

