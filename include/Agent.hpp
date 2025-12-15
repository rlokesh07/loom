#ifndef AGENT_HPP
#define AGENT_HPP

#include "Tool.hpp"
#include "CompletionEngine.hpp"
#include "ChatGPTCompletionEngine.hpp"
#include "OllamaCompletionEngine.hpp"
#include "ToolRegistry.hpp"
#include <string>
#include <vector>
#include <memory>


/**
 * @brief Agentic wrapper for LLM completion engines
 *
 * This extends the base Tool class but adds agent-like capabilities:
 * - Maintains conversation history
 * - Can perform multi-turn interactions
 * - Has memory across multiple execute() calls
 * - Supports any CompletionEngine (ChatGPT, Ollama, etc.)
 */
// Struct to track tool calls made during execution
struct ToolCallInfo {
    std::string tool_name;
    std::string arguments;
    std::string result;
    bool success;
};

class Agent : public Tool {
private:
    std::shared_ptr<CompletionEngine> engine_;
    std::vector<ChatMessage> conversation_history_;
    bool verbose_;
    ToolRegistryPtr tool_registry_;
    std::string default_model_;  // Default model to use
    std::string sessionID_;
    std::vector<ToolCallInfo> last_tool_calls_;  // Track tool calls from last execution

public:
   Agent(bool verbose = false);

    // agent defaults to gpt 3.5 model if not specified
    Agent(std::shared_ptr<CompletionEngine> engine,
          const std::string& default_model = "gpt-3.5-turbo",
          bool verbose = false);

    // execute the model with the parameters
    ToolResult execute(const std::map<std::string, std::any>& params) override;


    bool has_api_key() const;

    //returns the completion engine
    std::shared_ptr<CompletionEngine> get_engine() const { return engine_; }

    // simple completion 
    ToolResult complete(const std::string& user_message,
                       const std::string& model = "",
                       double temperature = 0.7);
    
    //add message to history
    void add_message(const std::string& role, const std::string& content);

    void set_session_id(std::string sessionID);

    const std::vector<ChatMessage>& get_history() const { return conversation_history_; }

    void clear_history();

    void set_verbose(bool verbose) { verbose_ = verbose; }

    bool is_verbose() const { return verbose_; }

    void talk(Agent& agent2, std::map<std::string, std::any>& params1, std::map<std::string, std::any>& params2);

    // Tool registry methods
    void set_tool_registry(ToolRegistryPtr registry) { tool_registry_ = registry; }
    ToolRegistryPtr get_tool_registry() const { return tool_registry_; }
    bool has_tools() const { return tool_registry_ != nullptr; }

    // Get tool calls from last execution
    const std::vector<ToolCallInfo>& get_last_tool_calls() const { return last_tool_calls_; }

private:
    // basic llm call
    ToolResult execute_llm_call(const std::string& model, double temperature, int max_tokens);
    
    // process the tool calls from the llm
   void process_tool_calls(const json& tool_calls);
};

#endif // AGENT_HPP
