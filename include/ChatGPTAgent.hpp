#ifndef CHATGPT_AGENT_HPP
#define CHATGPT_AGENT_HPP

#include "Tool.hpp"
#include "ChatGPTCompletionEngine.hpp"
#include "ToolRegistry.hpp"
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
    ToolRegistryPtr tool_registry_;

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
     * - name (string): Name of the agent
     * - message (string): User message to add to conversation
     * - model (string, optional): Model to use (default: "gpt-3.5-turbo")
     * - temperature (number, optional): Sampling temperature 0-2 (default: 0.7)
     * - max_tokens (integer, optional): Maximum tokens to generate
     * - system_prompt (string, optional): System message (only used if conversation is empty)
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;


    bool has_api_key() const;

    //returns the gpt engine
    std::shared_ptr<ChatGPTCompletionEngine> get_engine() const { return engine_; }

    // helper function for more simple requests
    ToolResult complete(const std::string& user_message,
                       const std::string& model = "gpt-3.5-turbo",
                       double temperature = 0.7);
    
    //add message to history
    void add_message(const std::string& role, const std::string& content);

    const std::vector<ChatMessage>& get_history() const { return conversation_history_; }

    void clear_history();

    // set the output to verbose
    void set_verbose(bool verbose) { verbose_ = verbose; }

    bool is_verbose() const { return verbose_; }

    void talk(ChatGPTAgent& agent2, std::map<std::string, std::any>& params1, std::map<std::string, std::any>& params2);

    // Tool registry methods
    void set_tool_registry(ToolRegistryPtr registry) { tool_registry_ = registry; }
    ToolRegistryPtr get_tool_registry() const { return tool_registry_; }
    bool has_tools() const { return tool_registry_ != nullptr; }

private:
    /**
     * @brief Execute a single LLM call with tool support
     *
     * @param model Model to use
     * @param temperature Sampling temperature
     * @param max_tokens Max tokens to generate
     * @return ToolResult containing the response
     */
    ToolResult execute_llm_call(const std::string& model, double temperature, int max_tokens);

    /**
     * @brief Process tool calls from LLM response
     *
     * @param tool_calls JSON array of tool calls from the LLM
     */
    void process_tool_calls(const json& tool_calls);
};

#endif // CHATGPT_AGENT_HPP
