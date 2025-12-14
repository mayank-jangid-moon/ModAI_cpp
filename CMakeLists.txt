cmake_minimum_required(VERSION 3.20)
project(ModAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Widgets 
    Network 
    Charts
)

# Find nlohmann/json
find_package(nlohmann_json 3.2.0 REQUIRED)

# Optional: spdlog for logging
find_package(spdlog QUIET)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)


set(HEADERS
    include/core/ModerationEngine.h
    include/core/RuleEngine.h
    include/core/ContentItem.h
    include/network/HttpClient.h
    include/network/QtHttpClient.h
    include/network/RateLimiter.h
    include/detectors/TextDetector.h
    include/detectors/HFTextDetector.h
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
    include/ui/DetailPanel.h
    include/ui/RailguardOverlay.h
    include/ui/ReviewDialog.h
)

set(SOURCES
    src/main.cpp
    src/core/ModerationEngine.cpp
    src/core/RuleEngine.cpp
    src/core/ContentItem.cpp
    src/network/QtHttpClient.cpp
    src/network/RateLimiter.cpp
    src/detectors/HFTextDetector.cpp
    src/detectors/HiveImageModerator.cpp
    src/detectors/HiveTextModerator.cpp
    src/scraper/RedditScraper.cpp
    src/storage/JsonlStorage.cpp
    src/export/Exporter.cpp
    src/utils/Logger.cpp
    src/utils/Crypto.cpp
    src/ui/MainWindow.cpp
    src/ui/DashboardModel.cpp
    src/ui/DetailPanel.cpp
    src/ui/RailguardOverlay.cpp
    src/ui/ReviewDialog.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    nlohmann_json::nlohmann_json
)

# Optional: link spdlog if found
if(spdlog_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SPDLOG)
endif()

# Enable Qt MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Platform-specific settings
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Create data directory structure
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/cache)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/exports/reports)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/exports/csv)

# Tests (optional)
enable_testing()
add_subdirectory(tests OPTIONAL)

