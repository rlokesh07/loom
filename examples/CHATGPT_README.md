# ChatGPT Tool

A tool for interacting with OpenAI's ChatGPT API.

## Prerequisites

1. **Install libcurl development package:**
   ```bash
   sudo apt-get install libcurl4-openssl-dev
   ```

2. **Set your OpenAI API key:**
   ```bash
   export OPENAI_API_KEY='your-api-key-here'
   ```

   To make it permanent, add it to your `~/.bashrc` or `~/.zshrc`:
   ```bash
   echo "export OPENAI_API_KEY='your-api-key-here'" >> ~/.bashrc
   source ~/.bashrc
   ```

## Building

```bash
cd /home/alarcafil/personal/loom
mkdir build && cd build
cmake ..
cmake --build .
```

## Running

```bash
./build/chatgpt_example
```

## Usage

### Simple completion

```cpp
ChatGPTTool gpt;
auto result = gpt.complete("What is the capital of France?");

if (result.is_success()) {
    std::cout << result.get_message() << std::endl;
}
```

### Multi-turn conversation

```cpp
std::vector<ChatMessage> conversation = {
    ChatMessage("user", "What is AI?"),
    ChatMessage("assistant", "AI is artificial intelligence..."),
    ChatMessage("user", "Tell me more")
};

auto result = gpt.chat(conversation);
```

### With custom parameters

```cpp
json messages = json::array();
messages.push_back({
    {"role", "user"},
    {"content", "Translate hello to Spanish"}
});

std::map<std::string, std::any> params = {
    {"messages", messages},
    {"model", std::string("gpt-3.5-turbo")},
    {"temperature", 0.7},
    {"max_tokens", 100},
    {"system_prompt", std::string("You are a translator")}
};

auto result = gpt.execute_safe(params);
```

## Available Parameters

- **messages** (required): Array of message objects with "role" and "content"
- **model** (optional): Model to use (default: "gpt-3.5-turbo")
  - Available models: "gpt-3.5-turbo", "gpt-4", "gpt-4-turbo", etc.
- **temperature** (optional): 0.0-2.0, controls randomness (default: 0.7)
- **max_tokens** (optional): Maximum tokens to generate
- **system_prompt** (optional): System message to set context

## Features

- ✅ Automatic API key loading from environment
- ✅ Full parameter control (model, temperature, max_tokens)
- ✅ Multi-turn conversations
- ✅ System prompts for context
- ✅ Error handling
- ✅ Token usage tracking
- ✅ JSON response parsing

## Example Output

```
=== ChatGPT Tool Example ===

API Key: Set ✓

Example 1: Simple Completion
Prompt: What is the capital of France?

Success: true
Response: The capital of France is Paris.

Usage:
  Prompt tokens: 15
  Completion tokens: 8
  Total tokens: 23
```
