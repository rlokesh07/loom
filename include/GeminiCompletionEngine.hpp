#ifndef GEMINI_COMPLETION_ENGINE_HPP
#define GEMINI_COMPLETION_ENGINE_HPP

#include "CompletionEngine.hpp"
#include "ToolRegistry.hpp"
#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Engine for handling Google Gemini API completions
 *
 * This class encapsulates all the logic for making API requests
 * to Google's Gemini API. It supports chat completions and function calling.
 *
 * Note: Gemini API uses a different message format than OpenAI, so this
 * engine handles the conversion from ChatMessage format to Gemini's format.
 */
class GeminiCompletionEngine : public CompletionEngine {
private:
    std::string api_key_;
    std::string base_url_;

    /**
     * @brief Load API key from environment variable
     */
    std::string load_api_key_from_env() const;

    /**
     * @brief Convert ChatMessage format to Gemini's format
     *
     * Gemini uses a different structure:
     * - Messages have "role" (user/model) and "parts" array
     * - System instructions are separate from messages
     *
     * @param messages Vector of ChatMessages in standard format
     * @return Tuple of (system_instruction, gemini_messages_array)
     */
    std::pair<std::string, json> convert_messages_to_gemini_format(
        const std::vector<ChatMessage>& messages) const;

public:
    /**
     * @brief Construct a new Gemini Completion Engine
     *
     * Automatically loads API key from GEMINI_API_KEY environment variable
     */
    GeminiCompletionEngine();

    /**
     * @brief Check if API key is set
     */
    bool has_api_key() const;

    /**
     * @brief Make HTTP POST request to Gemini API
     *
     * @param request_body JSON request body for the API
     * @return Response string from the API
     */
    std::string make_request(const json& request_body) override;

    /**
     * @brief Build a request body for the Gemini API
     *
     * @param messages Vector of chat messages
     * @param model Model to use (e.g., "gemini-1.5-pro", "gemini-1.5-flash")
     * @param temperature Sampling temperature (0.0 to 2.0)
     * @param max_tokens Maximum tokens to generate (optional, -1 for no limit)
     * @param registryPtr Tool registry for function calling (optional)
     * @param tool_choice Tool choice strategy (optional, "auto", "none")
     * @return JSON request body ready for API call
     */
    json build_request_body(const std::vector<ChatMessage>& messages,
                           const std::string& model = "gemini-1.5-flash",
                           double temperature = 0.7,
                           int max_tokens = -1,
                           ToolRegistryPtr registryPtr = nullptr,
                           const std::string& tool_choice = "auto") const override;
};

#endif // GEMINI_COMPLETION_ENGINE_HPP
