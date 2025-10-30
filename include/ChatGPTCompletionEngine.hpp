#ifndef CHATGPT_COMPLETION_ENGINE_HPP
#define CHATGPT_COMPLETION_ENGINE_HPP

#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Message structure for chat conversations
 */
struct ChatMessage {
    std::string role;     // "system", "user", or "assistant"
    std::string content;  // Message content

    ChatMessage(const std::string& role, const std::string& content)
        : role(role), content(content) {}

    json to_json() const {
        return {
            {"role", role},
            {"content", content}
        };
    }
};

/**
 * @brief Engine for handling OpenAI ChatGPT API completions
 *
 * This class encapsulates all the shared logic for making API requests
 * to OpenAI's ChatGPT API. It can be used by both simple tools and
 * agentic workflows to avoid code duplication.
 */
class ChatGPTCompletionEngine {
private:
    std::string api_key_;
    std::string base_url_;

    /**
     * @brief Load API key from environment variable
     */
    std::string load_api_key_from_env() const;

public:
    /**
     * @brief Construct a new ChatGPT Completion Engine
     *
     * Automatically loads API key from OPENAI_API_KEY environment variable
     */
    ChatGPTCompletionEngine();

    /**
     * @brief Check if API key is set
     */
    bool has_api_key() const;

    /**
     * @brief Extract messages from a std::any parameter
     */
    std::vector<ChatMessage> extract_messages(const std::any& messages_param) const;

    /**
     * @brief Make HTTP POST request to OpenAI API
     *
     * @param request_body JSON request body for the API
     * @return Response string from the API
     */
    std::string make_api_request(const json& request_body) const;

    /**
     * @brief Build a request body for the ChatGPT API
     *
     * @param messages Vector of chat messages
     * @param model Model to use (e.g., "gpt-3.5-turbo", "gpt-4")
     * @param temperature Sampling temperature (0.0 to 2.0)
     * @param max_tokens Maximum tokens to generate (optional, -1 for no limit)
     * @return JSON request body ready for API call
     */
    json build_request_body(const std::vector<ChatMessage>& messages,
                           const std::string& model = "gpt-3.5-turbo",
                           double temperature = 0.7,
                           int max_tokens = -1) const;

    /**
     * @brief Parse API response and extract completion text
     *
     * @param response_str Raw API response string
     * @param out_full_response Output parameter for full JSON response
     * @return Extracted completion text or error message
     */
    std::string parse_completion_response(const std::string& response_str,
                                         json& out_full_response) const;
};

#endif // CHATGPT_COMPLETION_ENGINE_HPP
