#include "../include/ChatGPTAgent.hpp"
#include <iostream>

ChatGPTAgent::ChatGPTAgent(bool verbose)
    : Tool("chatgpt_agent", "Agentic ChatGPT wrapper with conversation memory"),
      engine_(std::make_shared<ChatGPTCompletionEngine>()),
      verbose_(verbose) {

    // Define parameters - simpler than ChatGPTTool since we maintain history
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

bool ChatGPTAgent::has_api_key() const {
    return engine_->has_api_key();
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

        // Extract model (default: gpt-3.5-turbo)
        std::string model = "gpt-3.5-turbo";
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

        // Build request body using the engine
        json request_body = engine_->build_request_body(
            conversation_history_,
            model,
            temperature,
            max_tokens
        );

        // Make API request
        std::string response_str = engine_->make_api_request(request_body);

        // Parse response
        json full_response;
        std::string completion = engine_->parse_completion_response(response_str, full_response);

        // Add assistant response to history
        conversation_history_.emplace_back("assistant", completion);

        // Print to cout if verbose mode is enabled
        if (verbose_) {
            std::cout << "Assistant: " << completion << std::endl;
        }

        // Return full response as data for advanced usage
        return ToolResult::Success(full_response, completion);

    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}

ToolResult ChatGPTAgent::complete(const std::string& user_message,
                                  const std::string& model,
                                  double temperature) {
    std::map<std::string, std::any> params = {
        {"message", user_message},
        {"model", model},
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
