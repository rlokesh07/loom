#ifndef CHATGPT_TOOL_HPP
#define CHATGPT_TOOL_HPP

#include "Tool.hpp"
#include "ChatGPTCompletionEngine.hpp"
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Tool for interacting with OpenAI's ChatGPT API
 *
 * Automatically reads API key from OPENAI_API_KEY environment variable.
 * Supports chat completions with configurable parameters.
 */
class ChatGPTTool : public Tool {
private:
    std::shared_ptr<ChatGPTCompletionEngine> engine_;

public:
    /**
     * @brief Construct a new ChatGPTTool
     *
     * Automatically loads API key from OPENAI_API_KEY environment variable
     */
    ChatGPTTool();

    /**
     * @brief Execute chat completion
     *
     * Parameters:
     * - messages (array): Array of message objects with "role" and "content"
     * - model (string, optional): Model to use (default: "gpt-3.5-turbo")
     * - temperature (number, optional): Sampling temperature 0-2 (default: 0.7)
     * - max_tokens (integer, optional): Maximum tokens to generate
     * - system_prompt (string, optional): System message to set context
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

    /**
     * @brief Check if API key is set
     */
    bool has_api_key() const;

    /**
     * @brief Get the completion engine
     */
    std::shared_ptr<ChatGPTCompletionEngine> get_engine() const { return engine_; }

    /**
     * @brief Simple helper for single message completion
     */
    ToolResult complete(const std::string& user_message,
                       const std::string& model = "gpt-3.5-turbo",
                       double temperature = 0.7);

    /**
     * @brief Helper for multi-message conversation
     */
    ToolResult chat(const std::vector<ChatMessage>& messages,
                   const std::string& model = "gpt-3.5-turbo",
                   double temperature = 0.7);
};

#endif // CHATGPT_TOOL_HPP
