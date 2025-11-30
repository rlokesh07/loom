#ifndef OLLAMA_COMPLETION_ENGINE_HPP
#define OLLAMA_COMPLETION_ENGINE_HPP

#include "CompletionEngine.hpp"
#include "ToolRegistry.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Engine for handling Ollama API completions
 *
 * This class provides integration with Ollama's OpenAI-compatible API
 * for local LLM inference. It supports chat completions, tool calling,
 * and structured outputs.
 */
class OllamaCompletionEngine : public CompletionEngine{
public:
    /**
     * @brief Construct a new Ollama Completion Engine
     *
     * Uses http://localhost:11434/v1/chat/completions by default
     * Can be overridden with OLLAMA_BASE_URL environment variable
     */
    OllamaCompletionEngine();

    /**
     * @brief Construct with custom base URL
     *
     * @param base_url Custom Ollama API endpoint
     */
    OllamaCompletionEngine(const std::string& base_url);

    /**
     * @brief Make HTTP POST request to Ollama API
     *
     * @param request_body JSON request body for the API
     * @return Response string from the API
     */
    std::string make_request(const json& request_body) override;

    /**
     * @brief Build a request body for the Ollama API
     *
     * @param messages Vector of chat messages
     * @param model Model to use (e.g., "llama3.2", "mistral", "codellama")
     * @param temperature Sampling temperature (0.0 to 2.0)
     * @param max_tokens Maximum tokens to generate (optional, -1 for no limit)
     * @param tools JSON array of tool schemas (optional, for function calling)
     * @param tool_choice Tool choice strategy (optional, "auto", "none", or specific tool)
     * @return JSON request body ready for API call
     */
    json build_request_body(const std::vector<ChatMessage>& messages,
                           const std::string& model = "llama3.2",
                           double temperature = 0.7,
                           int max_tokens = -1,
                           ToolRegistryPtr registryPtr = nullptr,
                           const std::string& tool_choice = "auto") const override;

private:
    std::string base_url_;
};

#endif // OLLAMA_COMPLETION_ENGINE_HPP
