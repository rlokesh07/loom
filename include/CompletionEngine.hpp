#ifndef COMPLETION_ENGINE_HPP
#define COMPLETION_ENGINE_HPP

#include "ToolRegistry.hpp"
#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Token usage metrics from LLM API responses
 */
struct TokenUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
};

/**
 * @brief Message structure for chat conversations
 */
struct ChatMessage {
    std::string role;           // "system", "user", "assistant", or "tool"
    std::string content;        // Message content
    std::string tool_call_id;   // ID for tool call messages (role="tool")
    json tool_calls;            // Tool calls from assistant (role="assistant")

    ChatMessage(const std::string& role, const std::string& content)
        : role(role), content(content), tool_call_id(""), tool_calls(nullptr) {}

    json to_json() const {
        json msg = {
            {"role", role}
        };

        // Add content if not empty (tool messages require content)
        if (!content.empty()) {
            msg["content"] = content;
        }

        // Add tool_call_id for tool role messages
        if (role == "tool" && !tool_call_id.empty()) {
            msg["tool_call_id"] = tool_call_id;
        }

        // Add tool_calls for assistant messages with tool calls
        if (role == "assistant" && !tool_calls.is_null()) {
            msg["tool_calls"] = tool_calls;
        }

        return msg;
    }
};

// base class for the llm modelds
class CompletionEngine {
public:
    virtual ~CompletionEngine() = default;

    // extract messages from a std::any parameter
    std::vector<ChatMessage> extract_messages(const std::any& messages_param) const;

    // setting up the basic make request function that all engines need
    virtual std::string make_request(const json& request_body) = 0;


    // each models way of creating the request body
    virtual json build_request_body(const std::vector<ChatMessage>& messages,
                                   const std::string& model,
                                   double temperature = 0.7,
                                   int max_tokens = -1,
                                   ToolRegistryPtr registryPtr = nullptr,
                                   const std::string& tool_choice = "auto") const = 0;

    /**
     * @brief Parse API response and extract completion text
     *
     * Handles OpenAI-compatible response format used by both OpenAI and Ollama
     *
     * @param response_str Raw API response string
     * @param out_full_response Output parameter for full JSON response
     * @param out_usage Optional output parameter for token usage metrics (default: nullptr)
     * @return Extracted completion text or error message
     */
    std::string parse_completion_response(const std::string& response_str,
                                         json& out_full_response,
                                         TokenUsage* out_usage = nullptr) const;

protected:
    // Model context window limits (in tokens)
    static const std::map<std::string, int> MODEL_CONTEXT_LIMITS;

    /**
     * @brief Truncate messages to fit within model context window
     * Preserves system messages and keeps most recent messages
     */
    static std::vector<ChatMessage> truncate_messages(
        const std::vector<ChatMessage>& messages,
        const std::string& model,
        int max_completion_tokens
    );
};

#endif // COMPLETION_ENGINE_HPP
