#include "../include/ChatGPTTool.hpp"

ChatGPTTool::ChatGPTTool()
    : Tool("chatgpt", "Interact with OpenAI's ChatGPT API for text generation and conversation"),
      engine_(std::make_shared<ChatGPTCompletionEngine>()) {

    // Define parameters
    add_parameter(Parameter("messages", ParameterType::ARRAY,
                           "Array of message objects with 'role' and 'content'", true));

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
                                 "System message to set conversation context", false);
    add_parameter(system_prompt_param);
}

bool ChatGPTTool::has_api_key() const {
    return engine_->has_api_key();
}

ToolResult ChatGPTTool::execute(const std::map<std::string, std::any>& params) {
    // Check API key
    if (!has_api_key()) {
        return ToolResult::Error("OPENAI_API_KEY environment variable not set");
    }

    try {
        // Extract messages
        auto messages_it = params.find("messages");
        if (messages_it == params.end()) {
            return ToolResult::Error("Missing required parameter: messages");
        }

        std::vector<ChatMessage> messages = engine_->extract_messages(messages_it->second);

        // Add system prompt if provided
        auto system_it = params.find("system_prompt");
        if (system_it != params.end()) {
            std::string system_prompt = std::any_cast<std::string>(system_it->second);
            messages.insert(messages.begin(), ChatMessage("system", system_prompt));
        }

        if (messages.empty()) {
            return ToolResult::Error("No valid messages provided");
        }

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
            messages,
            model,
            temperature,
            max_tokens
        );

        // Make API request
        std::string response_str = engine_->make_request(request_body);

        // Parse response
        json full_response;
        std::string completion = engine_->parse_completion_response(response_str, full_response);

        // Return full response as data for advanced usage
        return ToolResult::Success(full_response, completion);

    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}

ToolResult ChatGPTTool::complete(const std::string& user_message,
                                const std::string& model,
                                double temperature) {
    json messages = json::array();
    messages.push_back({
        {"role", "user"},
        {"content", user_message}
    });

    std::map<std::string, std::any> params = {
        {"messages", messages},
        {"model", model},
        {"temperature", temperature}
    };

    return execute_safe(params);
}

ToolResult ChatGPTTool::chat(const std::vector<ChatMessage>& messages,
                            const std::string& model,
                            double temperature) {
    json messages_json = json::array();
    for (const auto& msg : messages) {
        messages_json.push_back(msg.to_json());
    }

    std::map<std::string, std::any> params = {
        {"messages", messages_json},
        {"model", model},
        {"temperature", temperature}
    };

    return execute_safe(params);
}
