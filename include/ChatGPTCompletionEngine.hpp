#ifndef CHATGPT_COMPLETION_ENGINE_HPP
#define CHATGPT_COMPLETION_ENGINE_HPP

#include "CompletionEngine.hpp"
#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Engine for handling OpenAI ChatGPT API completions
 *
 * This class encapsulates all the logic for making API requests
 * to OpenAI's ChatGPT API. It can be used by both simple tools and
 * agentic workflows to avoid code duplication.
 */
class ChatGPTCompletionEngine : public CompletionEngine {
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
     * @brief Make HTTP POST request to OpenAI API
     *
     * @param request_body JSON request body for the API
     * @return Response string from the API
     */
    std::string make_request(const json& request_body) override;

    /**
     * @brief Build a request body for the ChatGPT API
     *
     * @param messages Vector of chat messages
     * @param model Model to use (e.g., "gpt-3.5-turbo", "gpt-4")
     * @param temperature Sampling temperature (0.0 to 2.0)
     * @param max_tokens Maximum tokens to generate (optional, -1 for no limit)
     * @param tools JSON array of tool schemas (optional, for function calling)
     * @param tool_choice Tool choice strategy (optional, "auto", "none", or specific tool)
     * @return JSON request body ready for API call
     */
    json build_request_body(const std::vector<ChatMessage>& messages,
                           const std::string& model = "gpt-3.5-turbo",
                           double temperature = 0.7,
                           int max_tokens = -1,
                           const json& tools = json(nullptr),
                           const std::string& tool_choice = "auto") const override;
};

#endif // CHATGPT_COMPLETION_ENGINE_HPP
