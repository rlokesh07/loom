#ifndef CHATGPT_AGENT_HPP
#define CHATGPT_AGENT_HPP

#include "Tool.hpp"
#include "ChatGPTCompletionEngine.hpp"
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Agentic wrapper for ChatGPT that can maintain conversation state
 *
 * This extends the base Tool class but adds agent-like capabilities:
 * - Maintains conversation history
 * - Can perform multi-turn interactions
 * - Has memory across multiple execute() calls
 */
class ChatGPTAgent : public Tool {
private:
    std::shared_ptr<ChatGPTCompletionEngine> engine_;
    std::vector<ChatMessage> conversation_history_;
    bool verbose_;

public:
    /**
     * @brief Construct a new ChatGPTAgent
     *
     * Automatically loads API key from OPENAI_API_KEY environment variable
     * @param verbose If true, all responses will be printed to cout
     */
    ChatGPTAgent(bool verbose = false);

    /**
     * @brief Execute chat completion with conversation history
     *
     * Parameters:
     * - message (string): User message to add to conversation
     * - model (string, optional): Model to use (default: "gpt-3.5-turbo")
     * - temperature (number, optional): Sampling temperature 0-2 (default: 0.7)
     * - max_tokens (integer, optional): Maximum tokens to generate
     * - system_prompt (string, optional): System message (only used if conversation is empty)
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
     * @brief Add a message to the conversation history
     */
    void add_message(const std::string& role, const std::string& content);

    /**
     * @brief Get the conversation history
     */
    const std::vector<ChatMessage>& get_history() const { return conversation_history_; }

    /**
     * @brief Clear conversation history
     */
    void clear_history();

    /**
     * @brief Set verbose mode
     */
    void set_verbose(bool verbose) { verbose_ = verbose; }

    /**
     * @brief Get verbose mode status
     */
    bool is_verbose() const { return verbose_; }
};

#endif // CHATGPT_AGENT_HPP
