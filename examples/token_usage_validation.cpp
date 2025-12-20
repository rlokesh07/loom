#include "../include/CompletionEngine.hpp"
#include <iostream>
#include <cassert>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Validation test for TokenUsage output parameter
 * Tests both backward compatibility and new functionality
 */

// Mock CompletionEngine for testing
class MockCompletionEngine : public CompletionEngine {
public:
    std::string make_request(const json& request_body) override {
        // Not used in this test
        return "";
    }

    json build_request_body(const std::vector<ChatMessage>& messages,
                           const std::string& model,
                           double temperature,
                           int max_tokens,
                           ToolRegistryPtr registryPtr,
                           const std::string& tool_choice) const override {
        // Not used in this test
        return json::object();
    }
};

int main() {
    std::cout << "=== TokenUsage Validation Test ===" << std::endl;

    MockCompletionEngine engine;

    // Simulated OpenAI API response with usage data
    std::string response_str = R"({
        "id": "chatcmpl-test123",
        "object": "chat.completion",
        "created": 1234567890,
        "model": "gpt-4",
        "usage": {
            "prompt_tokens": 25,
            "completion_tokens": 150,
            "total_tokens": 175
        },
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "This is a test response."
            },
            "finish_reason": "stop"
        }]
    })";

    // Test 1: Backward compatibility (no usage parameter)
    std::cout << "\n[Test 1] Backward Compatibility (no usage param)" << std::endl;
    {
        json full_response;
        std::string completion = engine.parse_completion_response(response_str, full_response);

        assert(completion == "This is a test response.");
        assert(full_response.contains("usage"));
        std::cout << "  ✓ Existing signature still works" << std::endl;
        std::cout << "  ✓ Completion: " << completion << std::endl;
    }

    // Test 2: New functionality (with usage parameter)
    std::cout << "\n[Test 2] New Functionality (with usage param)" << std::endl;
    {
        json full_response;
        TokenUsage usage;
        std::string completion = engine.parse_completion_response(response_str, full_response, &usage);

        assert(completion == "This is a test response.");
        assert(usage.prompt_tokens == 25);
        assert(usage.completion_tokens == 150);
        assert(usage.total_tokens == 175);

        std::cout << "  ✓ Completion: " << completion << std::endl;
        std::cout << "  ✓ Prompt tokens: " << usage.prompt_tokens << std::endl;
        std::cout << "  ✓ Completion tokens: " << usage.completion_tokens << std::endl;
        std::cout << "  ✓ Total tokens: " << usage.total_tokens << std::endl;
    }

    // Test 3: Response without usage field (shouldn't crash)
    std::cout << "\n[Test 3] Response without usage field" << std::endl;
    {
        std::string response_no_usage = R"({
            "choices": [{
                "message": {
                    "role": "assistant",
                    "content": "Test without usage."
                },
                "finish_reason": "stop"
            }]
        })";

        json full_response;
        TokenUsage usage;
        std::string completion = engine.parse_completion_response(response_no_usage, full_response, &usage);

        assert(completion == "Test without usage.");
        assert(usage.prompt_tokens == 0);
        assert(usage.completion_tokens == 0);
        assert(usage.total_tokens == 0);

        std::cout << "  ✓ Gracefully handles missing usage field" << std::endl;
        std::cout << "  ✓ Default values: prompt=" << usage.prompt_tokens
                  << ", completion=" << usage.completion_tokens
                  << ", total=" << usage.total_tokens << std::endl;
    }

    // Test 4: Tool calls response (empty content)
    std::cout << "\n[Test 4] Tool calls response with usage" << std::endl;
    {
        std::string response_with_tools = R"({
            "usage": {
                "prompt_tokens": 100,
                "completion_tokens": 50,
                "total_tokens": 150
            },
            "choices": [{
                "message": {
                    "role": "assistant",
                    "tool_calls": [{"id": "call_1", "type": "function"}]
                },
                "finish_reason": "tool_calls"
            }]
        })";

        json full_response;
        TokenUsage usage;
        std::string completion = engine.parse_completion_response(response_with_tools, full_response, &usage);

        assert(completion == "");  // Empty for tool calls
        assert(usage.prompt_tokens == 100);
        assert(usage.completion_tokens == 50);
        assert(usage.total_tokens == 150);

        std::cout << "  ✓ Tool calls return empty content" << std::endl;
        std::cout << "  ✓ Usage still tracked: total=" << usage.total_tokens << std::endl;
    }

    std::cout << "\n=== All Tests Passed ✓ ===" << std::endl;
    std::cout << "\nValidation Summary:" << std::endl;
    std::cout << "  • Backward compatibility: CONFIRMED" << std::endl;
    std::cout << "  • Token extraction: WORKING" << std::endl;
    std::cout << "  • Null safety: VERIFIED" << std::endl;
    std::cout << "  • Edge cases: HANDLED" << std::endl;

    return 0;
}
