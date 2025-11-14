#include "../include/ChatGPTAgent.hpp"
#include <any>
#include <charconv>
#include <iostream>
#include <format>
ChatGPTAgent::ChatGPTAgent(bool verbose)
    : Tool("chatgpt_agent", "Agentic ChatGPT wrapper with conversation memory"),
      engine_(std::make_shared<ChatGPTCompletionEngine>()),
      verbose_(verbose),
      tool_registry_(nullptr),
      default_model_("gpt-3.5-turbo") {

    // Define parameters - simpler than ChatGPTTool since we maintain history
    add_parameter(Parameter("name", ParameterType::STRING, "Name of the agent", false));

    add_parameter(Parameter("message", ParameterType::STRING,
                           "User message to add to conversation", true));

    Parameter model_param("model", ParameterType::STRING,
                         "Model to use (e.g., gpt-3.5-turbo, gpt-4)", false);
    add_parameter(model_param);

    Parameter temp_param("temperature", ParameterType::FLOAT,
                        "Sampling temperature between 0 and 2", false);
    add_parameter(temp_param);

    Parameter max_tokens_param("max_tokens", ParameterType::INTEGER,
                              "Maximum number of tokens to generate", false);
    add_parameter(max_tokens_param);

    Parameter system_prompt_param("system_prompt", ParameterType::STRING,
                                 "System message (only used if conversation is empty)", false);
    add_parameter(system_prompt_param);

    Parameter verbose_param("verbose", ParameterType::BOOLEAN,
                           "If true, print responses to cout", false);
    add_parameter(verbose_param);
}

ChatGPTAgent::ChatGPTAgent(std::shared_ptr<CompletionEngine> engine,
                           const std::string& default_model,
                           bool verbose)
    : Tool("chatgpt_agent", "Agentic LLM wrapper with conversation memory"),
      engine_(engine),
      verbose_(verbose),
      tool_registry_(nullptr),
      default_model_(default_model) {

    // Define parameters
    add_parameter(Parameter("name", ParameterType::STRING, "Name of the agent", false));
    add_parameter(Parameter("message", ParameterType::STRING,
                           "User message to add to conversation", true));
    Parameter model_param("model", ParameterType::STRING,
                         "Model to use", false);
    add_parameter(model_param);
    Parameter temp_param("temperature", ParameterType::FLOAT,
                        "Sampling temperature between 0 and 2", false);
    add_parameter(temp_param);
    Parameter max_tokens_param("max_tokens", ParameterType::INTEGER,
                              "Maximum number of tokens to generate", false);
    add_parameter(max_tokens_param);
    Parameter system_prompt_param("system_prompt", ParameterType::STRING,
                                 "System message (only used if conversation is empty)", false);
    add_parameter(system_prompt_param);
    Parameter verbose_param("verbose", ParameterType::BOOLEAN,
                           "If true, print responses to cout", false);
    add_parameter(verbose_param);
}

bool ChatGPTAgent::has_api_key() const {
    // Try to cast to ChatGPTCompletionEngine to check for API key
    auto chatgpt_engine = std::dynamic_pointer_cast<ChatGPTCompletionEngine>(engine_);
    if (chatgpt_engine) {
        return chatgpt_engine->has_api_key();
    }
    // For other engines (like Ollama), assume they're always ready
    return true;
}

ToolResult ChatGPTAgent::execute(const std::map<std::string, std::any>& params) {
    // Check API key
    if (!has_api_key()) {
        return ToolResult::Error("OPENAI_API_KEY environment variable not set");
    }

    try {
        // Extract message
        auto message_it = params.find("message");
        if (message_it == params.end()) {
            return ToolResult::Error("Missing required parameter: message");
        }

        std::string user_message = std::any_cast<std::string>(message_it->second);

        // Check for verbose parameter override
        auto verbose_it = params.find("verbose");
        if (verbose_it != params.end()) {
            verbose_ = std::any_cast<bool>(verbose_it->second);
        }

        // Add system prompt if this is the first message and system prompt is provided
        auto system_it = params.find("system_prompt");
        if (conversation_history_.empty() && system_it != params.end()) {
            std::string system_prompt = std::any_cast<std::string>(system_it->second);
            conversation_history_.emplace_back("system", system_prompt);
        }

        // Add user message to history
        conversation_history_.emplace_back("user", user_message);

        // Extract model (use default model for this engine)
        std::string model = default_model_;
        auto model_it = params.find("model");
        if (model_it != params.end()) {
            model = std::any_cast<std::string>(model_it->second);
        }

        // Extract temperature (default: 0.7)
        double temperature = 0.7;
        auto temp_it = params.find("temperature");
        if (temp_it != params.end()) {
            temperature = std::any_cast<double>(temp_it->second);
        }

        // Extract max_tokens (default: -1, no limit)
        int max_tokens = -1;
        auto max_tokens_it = params.find("max_tokens");
        if (max_tokens_it != params.end()) {
            max_tokens = std::any_cast<int>(max_tokens_it->second);
        }

        // Execute LLM call with tool support (this handles tool calling loop)
        return execute_llm_call(model, temperature, max_tokens);

    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}


void ChatGPTAgent::talk(ChatGPTAgent& agent2, std::map<std::string, std::any>& params1, std::map<std::string, std::any>& params2){
    auto agent1Name_it = params1.find("name");
    auto agent2Name_it = params2.find("name");

    std::string agent1Name = std::any_cast<std::string>(agent1Name_it->second);
    std::string agent2Name = std::any_cast<std::string>(agent2Name_it->second);

    // Initialize agent1 with system prompt - agent1 starts the conversation
    params1.insert_or_assign("message", std::format("You are talking to {}. Start the conversation.", agent2Name));
    auto init1 = execute_safe(params1);
    if (!init1.is_success()) {
        std::cout << "✗ Agent 1 init failed: " << init1.get_message() << std::endl;
        return;
    }
    std::string agent1_msg = init1.get_message();

    // Initialize agent2 with system prompt only (no message yet)
    auto system_prompt_it = params2.find("system_prompt");
    if (system_prompt_it != params2.end()) {
        std::string system_prompt = std::any_cast<std::string>(system_prompt_it->second);
        agent2.add_message("system", system_prompt);
    }

    int MAX_TURNS = 100;
    std::string agent2_msg;

    for (int i = 0; i < MAX_TURNS; i++) {
        // Agent2 responds to agent1's message
        auto agent2Res = agent2.complete(agent1_msg);
        if (!agent2Res.is_success()) {
            auto error = agent2Res.get_error();
            std::cout << "✗ Agent 2 failed: " << (error.has_value() ? *error : "Unknown error") << std::endl;
            break;
        }
        agent2_msg = agent2Res.get_message();

        if (agent2_msg.find("COMPLETE") != std::string::npos ||
            agent1_msg.find("COMPLETE") != std::string::npos) {
            std::cout << "\n--- Conversation Complete ---" << std::endl;
            break;
        }

        // Agent1 responds to agent2's message
        auto agent1Res = complete(agent2_msg);
        if (!agent1Res.is_success()) {
            auto error = agent1Res.get_error();
            std::cout << "✗ Agent 1 failed: " << (error.has_value() ? *error : "Unknown error") << std::endl;
            break;
        }
        agent1_msg = agent1Res.get_message();

        if (agent1_msg.find("COMPLETE") != std::string::npos ||
            agent2_msg.find("COMPLETE") != std::string::npos) {
            std::cout << "\n--- Conversation Complete ---" << std::endl;
            break;
        }
    }
}

ToolResult ChatGPTAgent::complete(const std::string& user_message,
                                  const std::string& model,
                                  double temperature) {
    std::map<std::string, std::any> params = {
        {"message", user_message},
        {"model", model.empty() ? default_model_ : model},
        {"temperature", temperature}
    };

    return execute_safe(params);
}

void ChatGPTAgent::add_message(const std::string& role, const std::string& content) {
    conversation_history_.emplace_back(role, content);
}

void ChatGPTAgent::clear_history() {
    conversation_history_.clear();
}

ToolResult ChatGPTAgent::execute_llm_call(const std::string& model, double temperature, int max_tokens) {
    try {
        // Prepare tools schema if available
        json tools_schema = json(nullptr);
        if (has_tools() && tool_registry_->size() > 0) {
            tools_schema = tool_registry_->get_all_schemas();
        }

        // Build request body with tools (polymorphic call)
        json request_body = engine_->build_request_body(
            conversation_history_,
            model,
            temperature,
            max_tokens,
            tools_schema,
            "auto"
        );

        // Make API request (polymorphic call)
        std::string response_str = engine_->make_request(request_body);

        // Parse response
        json full_response;
        std::string completion = engine_->parse_completion_response(response_str, full_response);

        // Check if there are tool calls in the response
        if (full_response.contains("choices") && !full_response["choices"].empty()) {
            const auto& message = full_response["choices"][0]["message"];

            if (message.contains("tool_calls") && !message["tool_calls"].is_null()) {
                // Add assistant message with tool calls to history
                ChatMessage assistant_msg("assistant", "");
                assistant_msg.tool_calls = message["tool_calls"];
                conversation_history_.push_back(assistant_msg);

                if (verbose_) {
                    std::cout << "\nAssistant is calling tools..." << std::endl;

                    // Display each tool call
                    for (const auto& tool_call : message["tool_calls"]) {
                        std::string tool_name = tool_call["function"]["name"].get<std::string>();
                        std::string tool_args = tool_call["function"]["arguments"].get<std::string>();
                        std::cout << "  → Tool: " << tool_name << std::endl;
                        std::cout << "    Arguments: " << tool_args << std::endl;
                    }
                    std::cout << std::endl;
                }

                // Process tool calls
                process_tool_calls(message["tool_calls"]);

                // Make another LLM call with tool results
                return execute_llm_call(model, temperature, max_tokens);
            }
        }

        // No tool calls - add regular assistant response to history
        conversation_history_.emplace_back("assistant", completion);

        // Print to cout if verbose mode is enabled
        if (verbose_) {
            std::cout << "Assistant: " << completion << std::endl;
        }

        // Return full response as data for advanced usage
        return ToolResult::Success(full_response, completion);

    } catch (const std::exception& e) {
        return ToolResult::Error("LLM execution error: " + std::string(e.what()));
    }
}

void ChatGPTAgent::process_tool_calls(const json& tool_calls) {
    if (!has_tools()) {
        throw std::runtime_error("Tool calls requested but no tool registry available");
    }

    for (const auto& tool_call : tool_calls) {
        std::string tool_call_id = tool_call["id"].get<std::string>();
        std::string tool_name = tool_call["function"]["name"].get<std::string>();
        json tool_args = json::parse(tool_call["function"]["arguments"].get<std::string>());

        if (verbose_) {
            std::cout << "Executing: " << tool_name << "(" << tool_args.dump() << ")" << std::endl;
        }

        // Execute the tool
        ToolResult result = tool_registry_->execute_tool(tool_name, tool_args);

        // Prepare tool response message
        std::string tool_response;
        if (result.is_success()) {
            tool_response = result.get_message();
            if (verbose_) {
                std::cout << "  ✓ Result: " << tool_response << std::endl;
            }
        } else {
            auto error = result.get_error();
            tool_response = "Error: " + (error.has_value() ? *error : "Unknown error");
            if (verbose_) {
                std::cout << "  ✗ Error: " << tool_response << std::endl;
            }
        }

        // Add tool response to conversation history
        ChatMessage tool_msg("tool", tool_response);
        tool_msg.tool_call_id = tool_call_id;
        conversation_history_.push_back(tool_msg);
    }
}
