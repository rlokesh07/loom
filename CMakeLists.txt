cmake_minimum_required(VERSION 3.14)
project(loom VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(nlohmann_json 3.0.0 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Tool System Library
add_library(tool_system STATIC
    src/Parameter.cpp
    src/ToolResult.cpp
    src/Tool.cpp
    src/Agent.cpp
    src/CompletionEngine.cpp
    src/ChatGPTCompletionEngine.cpp
    src/OllamaCompletionEngine.cpp
    src/GeminiCompletionEngine.cpp
    src/ToolRegistry.cpp
    src/CalculatorTool.cpp
    src/GitHubTool.cpp
    src/Base64Tool.cpp
    src/WebSearchTool.cpp
    src/WebFetchTool.cpp
    src/SupabaseClient.cpp
    src/AuthManager.cpp
)

target_include_directories(tool_system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(tool_system PUBLIC
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# HTTP library for API server
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)



# Stateless API Server (with Supabase)
add_executable(loom_api_stateless
    src/api_main_stateless.cpp
    src/ApiServer_stateless.cpp
    src/AvailableToolsRegistry.cpp
)

target_link_libraries(loom_api_stateless PRIVATE
    tool_system
    httplib::httplib
)

# ChatGPT Agent Test
add_executable(chatgpt_agent_test
    examples/chatgpt_agent_test.cpp
    examples/CalculatorTool.cpp
)

target_link_libraries(chatgpt_agent_test PRIVATE
    tool_system
)
