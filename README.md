# Tool System

A flexible, type-safe tool execution framework for C++ agents and LLM integrations.

## Features

- **Type-safe parameter validation**: Define parameters with types, descriptions, and custom validators
- **Error handling**: Comprehensive error handling with detailed error messages
- **JSON schema generation**: Automatic generation of JSON schemas for LLM function calling
- **State management**: Built-in state management for stateful tools
- **Extensible**: Easy to create custom tools by inheriting from the base `Tool` class

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

## Building

### Prerequisites
- CMake 3.15 or higher
- C++17 compatible compiler
- nlohmann/json library

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Run example
./calculator_example
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

## Usage Example

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
