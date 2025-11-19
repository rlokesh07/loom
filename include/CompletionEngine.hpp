#ifndef COMPLETION_ENGINE_HPP
#define COMPLETION_ENGINE_HPP

#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

/**
 * @brief Abstract base class for LLM completion engines
 *
 * This class provides a common interface for different LLM API providers
 * (OpenAI, Ollama, etc.). It encapsulates shared logic for message handling
 * and response parsing.
 */
class CompletionEngine {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~CompletionEngine() = default;

    /**
     * @brief Extract messages from a std::any parameter
     *
     * @param messages_param Messages in std::any format (typically json)
     * @return Vector of ChatMessage objects
     */
    std::vector<ChatMessage> extract_messages(const std::any& messages_param) const;

    /**
     * @brief Make HTTP POST request to the LLM API
     *
     * Pure virtual function - must be implemented by derived classes
     *
     * @param request_body JSON request body for the API
     * @return Response string from the API
     */
    virtual std::string make_request(const json& request_body) = 0;

    /**
     * @brief Build a request body for the API
     *
     * Pure virtual function - must be implemented by derived classes
     *
     * @param messages Vector of chat messages
     * @param model Model to use
     * @param temperature Sampling temperature (0.0 to 2.0)
     * @param max_tokens Maximum tokens to generate (optional, -1 for no limit)
     * @param tools JSON array of tool schemas (optional, for function calling)
     * @param tool_choice Tool choice strategy (optional, "auto", "none", or specific tool)
     * @return JSON request body ready for API call
     */
    virtual json build_request_body(const std::vector<ChatMessage>& messages,
                                   const std::string& model,
                                   double temperature = 0.7,
                                   int max_tokens = -1,
                                   const json& tools = json(nullptr),
                                   const std::string& tool_choice = "auto") const = 0;

    /**
     * @brief Parse API response and extract completion text
     *
     * Handles OpenAI-compatible response format used by both OpenAI and Ollama
     *
     * @param response_str Raw API response string
     * @param out_full_response Output parameter for full JSON response
     * @return Extracted completion text or error message
     */
    std::string parse_completion_response(const std::string& response_str,
                                         json& out_full_response) const;
};

#endif // COMPLETION_ENGINE_HPP
