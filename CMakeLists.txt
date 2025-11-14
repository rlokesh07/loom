cmake_minimum_required(VERSION 3.14)
project(loom VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(nlohmann_json 3.0.0 REQUIRED)
find_package(CURL REQUIRED)

# Tool System Library
add_library(tool_system STATIC
    src/Parameter.cpp
    src/ToolResult.cpp
    src/Tool.cpp
    src/ChatGPTTool.cpp
    src/ChatGPTAgent.cpp
    src/CompletionEngine.cpp
    src/ChatGPTCompletionEngine.cpp
    src/OllamaCompletionEngine.cpp
    src/ToolRegistry.cpp
    src/GitHubTool.cpp
    src/Base64Tool.cpp
)

target_include_directories(tool_system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(tool_system PUBLIC
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
)

# Original executable
set(SOURCES
    src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    tool_system
)

# Calculator Example
add_executable(calculator_example
    examples/main.cpp
    examples/CalculatorTool.cpp
)

target_link_libraries(calculator_example PRIVATE
    tool_system
)

# ChatGPT Example
add_executable(chatgpt_example
    examples/chatgpt_example.cpp
)

target_link_libraries(chatgpt_example PRIVATE
    tool_system
)

# ChatGPT Agent Test
add_executable(chatgpt_agent_test
    examples/chatgpt_agent_test.cpp
    examples/CalculatorTool.cpp
)

target_link_libraries(chatgpt_agent_test PRIVATE
    tool_system
)

# Base64 Example
add_executable(base64_example
    examples/base64_example.cpp
)

target_link_libraries(base64_example PRIVATE
    tool_system
)

# Ollama Test
add_executable(ollama_test
    examples/ollama_test.cpp
)

target_link_libraries(ollama_test PRIVATE
    tool_system
)
