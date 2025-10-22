#include "../include/ChatGPTTool.hpp"
#include <iostream>
#include <iomanip>

void print_divider() {
    std::cout << std::string(70, '=') << "\n\n";
}

void print_result(const ToolResult& result) {
    if (result.is_success()) {
        std::cout << "Success: true\n";
        std::cout << "Response: " << result.get_message() << "\n\n";

        // Try to extract full JSON response
        auto json_response = result.try_get_data_as<json>();
        if (json_response) {
            std::cout << "Usage:\n";
            if (json_response->contains("usage")) {
                auto usage = (*json_response)["usage"];
                std::cout << "  Prompt tokens: " << usage["prompt_tokens"] << "\n";
                std::cout << "  Completion tokens: " << usage["completion_tokens"] << "\n";
                std::cout << "  Total tokens: " << usage["total_tokens"] << "\n";
            }
        }
    } else {
        std::cout << "Success: false\n";
        if (result.get_error()) {
            std::cout << "Error: " << *result.get_error() << "\n";
        }
    }
    std::cout << "\n";
    print_divider();
}

int main() {
    std::cout << "=== ChatGPT Tool Example ===\n\n";

    // Create tool
    ChatGPTTool gpt;

    // Check if API key is set
    if (!gpt.has_api_key()) {
        std::cerr << "ERROR: OPENAI_API_KEY environment variable not set!\n";
        std::cerr << "Please set it with: export OPENAI_API_KEY='your-api-key-here'\n";
        return 1;
    }

    std::cout << "API Key: Set ✓\n\n";
    print_divider();

    // Example 1: Simple completion
    std::cout << "Example 1: Simple Completion\n";
    std::cout << "Prompt: What is the capital of France?\n\n";

    auto result1 = gpt.complete("What is the capital of France?");
    print_result(result1);

    // Example 2: With custom parameters
    std::cout << "Example 2: Creative Writing (higher temperature)\n";
    std::cout << "Prompt: Write a haiku about programming\n\n";

    auto result2 = gpt.complete(
        "Write a haiku about programming",
        "gpt-3.5-turbo",
        1.2  // Higher temperature for more creativity
    );
    print_result(result2);

    // Example 3: Multi-turn conversation
    std::cout << "Example 3: Multi-turn Conversation\n";
    std::cout << "Simulating a conversation about AI\n\n";

    std::vector<ChatMessage> conversation = {
        ChatMessage("user", "What is machine learning?"),
        ChatMessage("assistant", "Machine learning is a subset of AI that enables systems to learn from data."),
        ChatMessage("user", "Can you give me a simple example?")
    };

    auto result3 = gpt.chat(conversation);
    print_result(result3);

    // Example 4: Using execute with full parameter control
    std::cout << "Example 4: Direct Execute with JSON Messages\n";
    std::cout << "Using system prompt to set context\n\n";

    json messages = json::array();
    messages.push_back({
        {"role", "user"},
        {"content", "Translate 'Hello, how are you?' to Spanish"}
    });

    std::map<std::string, std::any> params = {
        {"messages", messages},
        {"model", std::string("gpt-3.5-turbo")},
        {"temperature", 0.3},  // Lower for more deterministic translation
        {"system_prompt", std::string("You are a professional translator.")}
    };

    auto result4 = gpt.execute_safe(params);
    print_result(result4);

    // Example 5: Code generation
    std::cout << "Example 5: Code Generation\n";
    std::cout << "Prompt: Write a Python function to calculate fibonacci\n\n";

    auto result5 = gpt.complete(
        "Write a Python function to calculate the nth Fibonacci number. Just show the code, no explanation.",
        "gpt-3.5-turbo",
        0.5
    );
    print_result(result5);

    // Example 6: Get tool schema
    std::cout << "Example 6: Tool Schema (for LLM integration)\n\n";
    json schema = gpt.get_schema();
    std::cout << schema.dump(2) << "\n\n";
    print_divider();

    return 0;
}
