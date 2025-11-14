# Loom Tool System Analysis: Phase 1 MCP Migration (Low-Risk Tools)

## Executive Summary
This document focuses specifically on Phase 1 of MCP migration: GitHubTool, Base64Tool, and CalculatorTool. These are stateless tools with no inter-tool dependencies, making them ideal for initial MCP server implementation.

---

## Part 1: Tool Class Structure Overview

### 1.1 Core Tool Architecture

**Base Class Location**: `/home/alarcafil/personal/loom/include/Tool.hpp`

```cpp
class Tool {
protected:
    std::string name_;
    std::string description_;
    std::vector<Parameter> parameters_;
    std::map<std::string, std::any> state_;  // Not used in Phase 1 tools

public:
    // Pure virtual - must be implemented by all tools
    virtual ToolResult execute(const std::map<std::string, std::any>& params) = 0;
    
    // Schema generation for LLM
    virtual json get_schema() const;
    
    // Execution with validation
    ToolResult execute_safe(const std::map<std::string, std::any>& params);
    ToolResult execute_from_json(const json& params);
};
```

### 1.2 Parameter Definition System

**Location**: `/home/alarcafil/personal/loom/include/Parameter.hpp`

```cpp
enum class ParameterType {
    STRING, INTEGER, FLOAT, BOOLEAN, ARRAY, OBJECT, ANY
};

class Parameter {
    std::string name_;
    ParameterType type_;
    std::string description_;
    bool required_;
    std::any default_value_;
    std::optional<std::function<bool(const std::any&)>> validator_;
};
```

### 1.3 Tool Result Wrapper

**Location**: `/home/alarcafil/personal/loom/include/ToolResult.hpp`

```cpp
class ToolResult {
    bool success_;
    std::any data_;
    std::string message_;
    std::optional<std::string> error_;
    
public:
    static ToolResult Success(const std::any& data, const std::string& message = "");
    static ToolResult Error(const std::string& error_message);
    json to_json() const;
};
```

---

## Part 2: ToolRegistry - Central Management Hub

**Location**: `/home/alarcafil/personal/loom/include/ToolRegistry.hpp`

```cpp
class ToolRegistry {
private:
    std::map<std::string, ToolPtr> tools_;

public:
    // Registration
    void register_tool(ToolPtr tool);
    template<typename T, typename... Args>
    std::shared_ptr<T> register_tool(Args&&... args);
    
    // Discovery
    ToolPtr get_tool(const std::string& name) const;
    bool has_tool(const std::string& name) const;
    std::vector<std::string> list_tools() const;
    
    // Schema aggregation
    json get_all_schemas() const;
    
    // Execution
    ToolResult execute_tool(const std::string& name, const json& params);
    
    // Management
    bool unregister_tool(const std::string& name);
    void clear();
    size_t size() const;
};
```

**Key Feature**: Tools are stored by name in a std::map. The registry generates combined JSON schemas compatible with OpenAI's function calling format.

---

## Part 3: Phase 1 Tools - Detailed Analysis

### 3.1 GitHubTool

**Location**: `/home/alarcafil/personal/loom/include/GitHubTool.hpp`

**Characteristics**:
- **Stateless**: No persistent state management
- **External API**: Uses libcurl for HTTP requests
- **Authentication**: Requires GITHUB_TOKEN environment variable
- **Base URL**: `https://api.github.com`

**Supported Operations**:
```
1. get_file
   - Required: owner, repo, path
   - Optional: branch (default: "main")
   - Returns: File content from repository

2. get_pull_requests
   - Required: owner, repo
   - Optional: state (open, closed, all - default: open)
   - Returns: List of pull request metadata

3. get_pull_request_details
   - Required: owner, repo, pr (pull request number)
   - Returns: Detailed PR information

4. get_pull_request_files
   - Required: owner, repo, pr (pull request number)
   - Returns: List of files changed in PR
```

**Parameters Definition** (from constructor):
```cpp
add_parameter(Parameter("operation", ParameterType::STRING,
    "The GitHub operation to perform (e.g., 'get_file', 'get_pull_requests', ...)", true));

add_parameter(Parameter("owner", ParameterType::STRING,
    "Repository owner (username or organization)", true));

add_parameter(Parameter("repo", ParameterType::STRING,
    "Repository name", true));

add_parameter(Parameter("path", ParameterType::STRING,
    "File path within the repository", false));

add_parameter(Parameter("branch", ParameterType::STRING,
    "Branch name (default: main)", false));

add_parameter(Parameter("pr", ParameterType::STRING,
    "Pull request number for get_pull_request_details", false));
```

**Key Implementation Details**:
- Uses `CURL_SLIST` for HTTP headers
- Authentication: `Authorization: Bearer {GITHUB_TOKEN}`
- User-Agent: `loom-github-tool`
- Accept header: `application/vnd.github.v3+json`

**Challenges for MCP**:
- HTTP requests need to work across process boundary
- libcurl state management
- Environment variable access (GITHUB_TOKEN)

---

### 3.2 Base64Tool

**Location**: `/home/alarcafil/personal/loom/include/Base64Tool.hpp`

**Characteristics**:
- **Stateless**: No persistent state
- **No External Dependencies**: Self-contained logic
- **Simple Operations**: Encode/decode only
- **Pure Functions**: No side effects

**Supported Operations**:
```
1. encode
   - Input: Plain text string
   - Output: Base64 encoded string

2. decode
   - Input: Base64 encoded string
   - Output: Plain text
```

**Parameters Definition**:
```cpp
// Implied from execute() method:
// - operation (string): "encode" or "decode" (required)
// - input (string): Text to encode or Base64 string to decode (required)
```

**Implementation Notes**:
```cpp
static constexpr char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::string& input) const;
std::string base64_decode(const std::string& input) const;
bool is_base64(unsigned char c) const;
```

**Challenges for MCP**:
- None - this is the simplest tool
- Pure logic, no external dependencies
- Ideal candidate for testing MCP implementation

---

### 3.3 CalculatorTool (Example)

**Location**: `/home/alarcafil/personal/loom/examples/CalculatorTool.hpp`

**Characteristics**:
- **Stateless**: No persistent state
- **Pure Math**: Simple arithmetic operations
- **Example Implementation**: Demonstrates tool pattern
- **Type Flexible**: Handles int, float, double parameters

**Supported Operations**:
```
1. add: a + b
2. subtract: a - b
3. multiply: a * b
4. divide: a / b (with zero-division check)
```

**Parameters Definition**:
```cpp
add_parameter(Parameter("operation", ParameterType::STRING,
    "The operation to perform: add, subtract, multiply, or divide", true));

add_parameter(Parameter("a", ParameterType::FLOAT,
    "First operand", true));

add_parameter(Parameter("b", ParameterType::FLOAT,
    "Second operand", true));
```

**Implementation Pattern** (all Phase 1 tools follow this):
```cpp
ToolResult CalculatorTool::execute(const std::map<std::string, std::any>& params) {
    try {
        // 1. Extract parameters with type conversion
        std::string operation = get_string_param(params.at("operation"));
        double a = get_number_param(params.at("a"));
        double b = get_number_param(params.at("b"));

        // 2. Validate inputs
        // 3. Perform operation
        // 4. Return result
        return ToolResult::Success(result, "Message...");
    } catch (const std::exception& e) {
        return ToolResult::Error(std::string("Error: ") + e.what());
    }
}
```

**Challenges for MCP**:
- None - ideal reference implementation

---

## Part 4: Tool Registration Pattern

### 4.1 Current Registration

```cpp
// Create registry
auto registry = std::make_shared<ToolRegistry>();

// Register Phase 1 tools
registry->register_tool<CalculatorTool>();
registry->register_tool<GitHubTool>();
registry->register_tool<Base64Tool>();

// Retrieve tools
auto calc = registry->get_tool("calculator");
auto github = registry->get_tool("github");
auto base64 = registry->get_tool("base64");

// Get all schemas for LLM
auto schemas = registry->get_all_schemas();
```

### 4.2 Schema Generation

The `get_schema()` method produces OpenAI-compatible format:

```json
{
    "name": "github",
    "description": "Interact with GitHub API to read files, manage issues, and more",
    "parameters": {
        "type": "object",
        "properties": {
            "operation": {
                "type": "string",
                "description": "The GitHub operation to perform..."
            },
            "owner": {
                "type": "string",
                "description": "Repository owner..."
            },
            "repo": {
                "type": "string",
                "description": "Repository name"
            },
            "path": {
                "type": "string",
                "description": "File path within the repository"
            }
        },
        "required": ["operation", "owner", "repo"]
    }
}
```

---

## Part 5: Execution Flow

### 5.1 Path 1: Direct Tool Execution

```cpp
auto tool = registry->get_tool("github");
std::map<std::string, std::any> params = {
    {"operation", std::string("get_file")},
    {"owner", std::string("anthropic")},
    {"repo", std::string("claude")},
    {"path", std::string("README.md")}
};
ToolResult result = tool->execute_safe(params);  // Includes validation
```

### 5.2 Path 2: JSON-based Execution

```cpp
json params = {
    {"operation", "get_file"},
    {"owner", "anthropic"},
    {"repo", "claude"},
    {"path", "README.md"}
};
ToolResult result = tool->execute_from_json(params);
```

### 5.3 Path 3: Registry Execution

```cpp
json params = {
    {"operation", "get_file"},
    {"owner", "anthropic"},
    {"repo", "claude"},
    {"path", "README.md"}
};
ToolResult result = registry->execute_tool("github", params);
```

### 5.4 Execution with Validation

```cpp
bool Tool::validate_parameters(const std::map<std::string, std::any>& params) const {
    for (const auto& param : parameters_) {
        // Check required parameters are present
        if (param.is_required() && params.find(param.get_name()) == params.end()) {
            return false;
        }
        
        // Validate each provided parameter
        auto it = params.find(param.get_name());
        if (it != params.end()) {
            if (!param.validate(it->second)) {
                return false;
            }
        }
    }
    return true;
}
```

---

## Part 6: Key Dependencies

### 6.1 External Libraries

For Phase 1 tools:

| Tool | Library | Purpose |
|------|---------|---------|
| GitHubTool | libcurl | HTTP API requests |
| Base64Tool | None | Self-contained |
| CalculatorTool | None | Self-contained |

**Core dependencies** (all tools):
- nlohmann_json (JSON parsing/generation)
- C++20 standard library

### 6.2 Build Configuration

```cmake
# From CMakeLists.txt
find_package(nlohmann_json 3.0.0 REQUIRED)
find_package(CURL REQUIRED)

add_library(tool_system STATIC
    src/Parameter.cpp
    src/ToolResult.cpp
    src/Tool.cpp
    src/ToolRegistry.cpp
    src/GitHubTool.cpp
    src/Base64Tool.cpp
)

target_link_libraries(tool_system PUBLIC
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
)
```

---

## Part 7: Parameter Types and Conversion

### 7.1 Supported Parameter Types

```cpp
enum class ParameterType {
    STRING,     // C++ std::string → JSON "string"
    INTEGER,    // C++ int → JSON "integer"
    FLOAT,      // C++ double → JSON "number"
    BOOLEAN,    // C++ bool → JSON "boolean"
    ARRAY,      // C++ json array → JSON "array"
    OBJECT,     // C++ json object → JSON "object"
    ANY         // Mixed types
};
```

### 7.2 JSON to C++ Conversion

```cpp
ToolResult Tool::execute_from_json(const json& params) {
    std::map<std::string, std::any> params_map;
    
    for (auto it = params.begin(); it != params.end(); ++it) {
        const auto& value = it.value();
        
        if (value.is_string()) {
            params_map[it.key()] = value.get<std::string>();
        } else if (value.is_number_integer()) {
            params_map[it.key()] = value.get<int>();
        } else if (value.is_number_float()) {
            params_map[it.key()] = value.get<double>();
        } else if (value.is_boolean()) {
            params_map[it.key()] = value.get<bool>();
        } else if (value.is_array() || value.is_object()) {
            params_map[it.key()] = value;  // Store as json
        }
    }
    
    return execute_safe(params_map);
}
```

---

## Part 8: Result Format

### 8.1 ToolResult Structure

```cpp
class ToolResult {
    bool success_;                           // Execution success flag
    std::any data_;                          // Result data (type-erased)
    std::string message_;                    // Human-readable message
    std::optional<std::string> error_;       // Error message if failed
};
```

### 8.2 Success Result Example

```cpp
ToolResult::Success(double(8.0), "Calculated: 5 + 3 = 8")
// Converts to JSON:
{
    "success": true,
    "message": "Calculated: 5 + 3 = 8",
    "data": 8.0,
    "error": null
}
```

### 8.3 Error Result Example

```cpp
ToolResult::Error("Division by zero")
// Converts to JSON:
{
    "success": false,
    "message": "",
    "data": null,
    "error": "Division by zero"
}
```

---

## Part 9: Phase 1 Tools - Quick Reference

### GitHubTool Properties
- **Name**: "github"
- **Requires**: GITHUB_TOKEN environment variable
- **Operations**: 4 (get_file, get_pull_requests, get_pull_request_details, get_pull_request_files)
- **Dependencies**: libcurl
- **Files**: `/home/alarcafil/personal/loom/include/GitHubTool.hpp`, `src/GitHubTool.cpp`

### Base64Tool Properties
- **Name**: "base64"
- **Requires**: None (no external deps)
- **Operations**: 2 (encode, decode)
- **Dependencies**: None (C++20 stdlib)
- **Files**: `/home/alarcafil/personal/loom/include/Base64Tool.hpp`, `src/Base64Tool.cpp`

### CalculatorTool Properties
- **Name**: "calculator"
- **Requires**: None
- **Operations**: 4 (add, subtract, multiply, divide)
- **Dependencies**: None (C++20 stdlib)
- **Files**: `/home/alarcafil/personal/loom/examples/CalculatorTool.hpp`, `examples/CalculatorTool.cpp`

---

## Part 10: Why Phase 1 is Ideal for MCP

### 10.1 No Inter-Tool Dependencies

Phase 1 tools operate independently:
- **GitHubTool**: Only depends on GitHub API
- **Base64Tool**: Pure logic, no external calls
- **CalculatorTool**: Pure math, no external calls

No tool calls another tool, so no registry dependency.

### 10.2 Stateless Operation

All Phase 1 tools are request-response:
- No conversation history
- No persistent memory
- No state between calls
- Perfect for stateless MCP servers

### 10.3 Clear Input/Output Contract

Each tool has:
- Well-defined parameters
- Clear expected outputs
- Error handling patterns
- JSON-serializable results

### 10.4 Minimal Integration Complexity

No need to handle:
- Tool calling loops (ChatGPTAgent feature)
- Conversation history management
- Cross-tool communication
- Stateful operations

---

## Part 11: MCP Server Implementation Outline for Phase 1

### 11.1 MCP Server Wrapper Structure

For each Phase 1 tool, create an MCP server:

```
MCP Server (GitHubTool)
├── Stdio-based JSON-RPC interface
├── Tool instance management
├── Parameter marshaling (JSON ↔ C++)
└── Response serialization

GitHub Tool (existing C++ class)
├── Parameter validation
├── API request logic
└── Result formatting
```

### 11.2 Key Integration Points

1. **Server Initialization**
   - Load environment variables (GITHUB_TOKEN)
   - Create tool instance
   - Expose tool schema

2. **Tool Call Request**
   - Receive JSON-RPC request with parameters
   - Convert JSON to C++ parameter map
   - Call `tool->execute()` or `tool->execute_from_json()`
   - Serialize result to JSON-RPC response

3. **Error Handling**
   - Catch exceptions from tool
   - Convert `ToolResult::Error()` to JSON-RPC error
   - Return standardized error response

### 11.3 Schema Conversion (OpenAI → MCP)

Current OpenAI format:
```json
{
    "name": "tool_name",
    "description": "...",
    "parameters": {
        "type": "object",
        "properties": {...},
        "required": [...]
    }
}
```

Can be adapted to MCP tool format (requires minimal changes).

---

## Part 12: Files to Review for Phase 1 Implementation

### Core Tool System (Read-Only for Reference)
- `/home/alarcafil/personal/loom/include/Tool.hpp` - Base class structure
- `/home/alarcafil/personal/loom/include/ToolRegistry.hpp` - Registry pattern
- `/home/alarcafil/personal/loom/include/Parameter.hpp` - Parameter definition
- `/home/alarcafil/personal/loom/include/ToolResult.hpp` - Result wrapper

### Phase 1 Tool Implementations
- `/home/alarcafil/personal/loom/include/GitHubTool.hpp` - Main implementation
- `/home/alarcafil/personal/loom/src/GitHubTool.cpp` - Full implementation (~200+ lines)
- `/home/alarcafil/personal/loom/include/Base64Tool.hpp` - Simple implementation
- `/home/alarcafil/personal/loom/src/Base64Tool.cpp` - Full implementation
- `/home/alarcafil/personal/loom/examples/CalculatorTool.hpp` - Reference example
- `/home/alarcafil/personal/loom/examples/CalculatorTool.cpp` - Reference implementation

### Build Configuration
- `/home/alarcafil/personal/loom/CMakeLists.txt` - Build setup

---

## Part 13: Key Code Patterns to Extract for MCP

### 13.1 Parameter Extraction Pattern

Used throughout tools:
```cpp
std::string get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}

double get_number_param(const std::any& value) const {
    if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    } else if (value.type() == typeid(int)) {
        return static_cast<double>(std::any_cast<int>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<double>(std::any_cast<float>(value));
    }
    throw std::bad_any_cast();
}
```

### 13.2 Error Handling Pattern

Standard across all tools:
```cpp
try {
    // Extract and validate parameters
    // Perform operation
    return ToolResult::Success(result, "Message...");
} catch (const std::exception& e) {
    return ToolResult::Error(std::string("Error: ") + e.what());
}
```

### 13.3 Parameter Definition Pattern

In tool constructor:
```cpp
add_parameter(Parameter("param_name", ParameterType::STRING,
                       "Human-readable description", true));  // true = required
```

---

## Part 14: Migration Checklist for Phase 1

### GitHubTool
- [ ] Understand GITHUB_TOKEN loading mechanism
- [ ] Document all 4 supported operations
- [ ] Review HTTP header construction
- [ ] Understand API response parsing
- [ ] Plan libcurl integration in MCP server

### Base64Tool
- [ ] Review base64_chars lookup table
- [ ] Understand encode/decode algorithms
- [ ] Document input validation (is_base64 checks)
- [ ] Plan error cases

### CalculatorTool
- [ ] Review all 4 arithmetic operations
- [ ] Understand division by zero handling
- [ ] Review type conversion logic
- [ ] Document expected error cases

### General
- [ ] Design JSON-RPC wrapper architecture
- [ ] Plan stdio communication layer
- [ ] Design parameter marshaling system
- [ ] Plan result serialization
- [ ] Design error response format

---

**Document Generated**: November 10, 2025
**Focus**: Phase 1 - Low Risk Tools (GitHubTool, Base64Tool, CalculatorTool)
**Codebase Version**: Latest (commit 6de7568)
**C++ Standard**: C++20
