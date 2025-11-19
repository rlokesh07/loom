# Tool System

A flexible, type-safe tool execution framework for C++ agents and LLM integrations.

## Features

- **Type-safe parameter validation**: Define parameters with types, descriptions, and custom validators
- **Error handling**: Comprehensive error handling with detailed error messages
- **JSON schema generation**: Automatic generation of JSON schemas for LLM function calling
- **State management**: Built-in state management for stateful tools
- **Extensible**: Easy to create custom tools by inheriting from the base `Tool` class
- **Built-in Tools**:
  - **WebSearchTool**: Search the web using Brave Search API
  - **WebFetchTool**: Fetch and read content from web URLs (no API key needed)
  - **GitHubTool**: Interact with GitHub repositories (read files, list PRs, etc.)
  - **Base64Tool**: Encode/decode base64 strings
  - **CalculatorTool**: Perform basic arithmetic operations

## Architecture

The system is organized into three main components:

### 1. Parameter (`Parameter.hpp`)
Defines tool parameters with:
- Name and description
- Type information (STRING, INTEGER, FLOAT, BOOLEAN, ARRAY, OBJECT, ANY)
- Required/optional flag
- Default values
- Custom validators

### 2. ToolResult (`ToolResult.hpp`)
Encapsulates execution results:
- Success/failure status
- Type-erased result data
- Status messages
- Error information
- JSON serialization

### 3. Tool (`Tool.hpp`)
Abstract base class for all tools:
- Parameter management
- Automatic validation
- JSON schema generation
- State persistence
- Safe execution wrapper

## Environment Variables

Some tools require API keys to be set as environment variables:

- **BRAVE_SEARCH_API_KEY**: Required for WebSearchTool
  - Get your free API key at: https://api.search.brave.com/app/keys
  - Free tier: 2,000 queries/month
- **GITHUB_TOKEN**: Required for GitHubTool
  - Create a personal access token at: https://github.com/settings/tokens
- **OPENAI_API_KEY**: Required for ChatGPT agent integration
  - Get your API key from: https://platform.openai.com/api-keys

Set environment variables in your shell:
```bash
export BRAVE_SEARCH_API_KEY='your-api-key-here'
export GITHUB_TOKEN='your-github-token-here'
export OPENAI_API_KEY='your-openai-key-here'
```

## Building

### Prerequisites
- CMake 3.15 or higher
- C++20 compatible compiler
- nlohmann/json library
- libcurl (for HTTP requests)

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run examples
./calculator_example
./websearch_example
./chatgpt_agent_test
./base64_example
```

### Installing nlohmann/json

**Ubuntu/Debian:**
```bash
sudo apt-get install nlohmann-json3-dev
```

**macOS:**
```bash
brew install nlohmann-json
```

**Manual installation:**
```bash
git clone https://github.com/nlohmann/json.git
cd json
mkdir build && cd build
cmake ..
sudo make install
```

## Usage Examples

### Using Built-in Tools

#### WebSearch Tool

```cpp
#include "WebSearchTool.hpp"

// Direct usage
WebSearchTool search_tool;

std::map<std::string, std::any> params = {
    {"query", std::string("latest AI developments")},
    {"count", 5}  // Optional: number of results (1-20, default 10)
};

ToolResult result = search_tool.execute(params);

if (result.is_success()) {
    json results = result.get_data_as<json>();
    std::cout << "Query: " << results["query"] << std::endl;

    for (const auto& item : results["results"]) {
        std::cout << item["title"] << std::endl;
        std::cout << item["url"] << std::endl;
        std::cout << item["description"] << std::endl;
    }
}
```

#### Agent Integration with WebSearch

```cpp
#include "Agent.hpp"
#include "ToolRegistry.hpp"
#include "WebSearchTool.hpp"

// Create agent and registry
Agent agent(true);  // Verbose mode
auto registry = std::make_shared<ToolRegistry>();

// Register tools
registry->register_tool<WebSearchTool>();
agent.set_tool_registry(registry);

// Agent will autonomously use web search when needed
std::map<std::string, std::any> params = {
    {"message", std::string("What are the latest developments in quantum computing?")},
    {"system_prompt", std::string("You are a research assistant with web search.")},
    {"model", std::string("gpt-4o-mini")}
};

auto result = agent.execute_safe(params);
// Agent will automatically search the web and summarize findings
```

#### WebFetch Tool

```cpp
#include "WebFetchTool.hpp"

// Direct usage
WebFetchTool fetch_tool;

std::map<std::string, std::any> params = {
    {"url", std::string("https://example.com/article")},
    {"max_length", 5000}  // Optional: max characters (default: 10000)
};

ToolResult result = fetch_tool.execute(params);

if (result.is_success()) {
    json data = result.get_data_as<json>();
    std::cout << "Content from: " << data["url"] << std::endl;
    std::cout << data["content"] << std::endl;  // HTML converted to plain text
}
```

#### Combined WebSearch + WebFetch

```cpp
#include "Agent.hpp"
#include "ToolRegistry.hpp"
#include "WebSearchTool.hpp"
#include "WebFetchTool.hpp"

Agent agent(true);
auto registry = std::make_shared<ToolRegistry>();

// Register both tools for full research capability
registry->register_tool<WebSearchTool>();
registry->register_tool<WebFetchTool>();
agent.set_tool_registry(registry);

std::map<std::string, std::any> params = {
    {"message", std::string("Research recent C++ developments, find articles, and summarize the key points from the top result.")},
    {"system_prompt", std::string("You are a research assistant. Use web_search to find pages, then web_fetch to read their content.")},
    {"model", std::string("gpt-4o-mini")}
};

auto result = agent.execute_safe(params);
// Agent will: 1) Search web, 2) Fetch top URL, 3) Read content, 4) Summarize
```

### Creating Custom Tools

```cpp
#include "Tool.hpp"

class MyTool : public Tool {
public:
    MyTool() : Tool("my_tool", "Description of my tool") {
        // Define parameters
        add_parameter(Parameter("input", ParameterType::STRING,
                               "Input string", true));
        add_parameter(Parameter("count", ParameterType::INTEGER,
                               "Number of repetitions", false));
    }

    ToolResult execute(const std::map<std::string, std::any>& params) override {
        // Extract parameters
        std::string input = std::any_cast<std::string>(params.at("input"));
        int count = params.count("count") ?
            std::any_cast<int>(params.at("count")) : 1;

        // Perform operation
        std::string result = "";
        for (int i = 0; i < count; i++) {
            result += input;
        }

        return ToolResult::Success(result, "Operation completed");
    }
};

// Usage
MyTool tool;

// Get schema for LLM
json schema = tool.get_schema();

// Execute with parameters
std::map<std::string, std::any> params = {
    {"input", std::string("Hello ")},
    {"count", 3}
};

ToolResult result = tool.execute_safe(params);

if (result.is_success()) {
    std::string output = result.get_data_as<std::string>();
    std::cout << output << std::endl;  // "Hello Hello Hello "
}
```

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── include/
│   ├── Parameter.hpp      # Parameter definitions
│   ├── ToolResult.hpp     # Result encapsulation
│   └── Tool.hpp           # Base tool class
├── src/
│   ├── Parameter.cpp      # Parameter implementation
│   ├── ToolResult.cpp     # ToolResult implementation
│   └── Tool.cpp           # Tool implementation
└── examples/
    ├── CalculatorTool.hpp # Example tool header
    ├── CalculatorTool.cpp # Example tool implementation
    └── main.cpp           # Example usage
```

## Creating Custom Tools

1. **Inherit from Tool**:
```cpp
class MyCustomTool : public Tool {
public:
    MyCustomTool() : Tool("my_tool", "Description") {
        // Define parameters
    }

    ToolResult execute(const std::map<std::string, std::any>& params) override {
        // Implementation
    }
};
```

2. **Define Parameters**:
```cpp
add_parameter(Parameter("param_name", ParameterType::STRING,
                       "Parameter description", true));
```

3. **Implement Execute**:
```cpp
ToolResult execute(const std::map<std::string, std::any>& params) override {
    try {
        // Extract and validate parameters
        // Perform operation
        return ToolResult::Success(result_data, "Success message");
    } catch (const std::exception& e) {
        return ToolResult::Error(e.what());
    }
}
```

## API Reference

### Tool
- `execute_safe(params)` - Execute with automatic validation
- `execute_from_json(json_params)` - Execute with JSON parameters
- `get_schema()` - Get JSON schema for LLM
- `set_state(key, value)` - Set persistent state
- `get_state(key)` - Get state value

### ToolResult
- `is_success()` - Check if execution succeeded
- `get_data_as<T>()` - Extract typed result data
- `try_get_data_as<T>()` - Safely try to extract data
- `to_json()` - Convert to JSON

### Parameter
- Constructor accepts name, type, description, required flag
- `set_default_value(value)` - Set default value
- `set_validator(func)` - Add custom validator

## License

MIT License - see LICENSE file for details
