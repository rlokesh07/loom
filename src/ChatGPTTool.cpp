#include "ChatGPTTool.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

ChatGPTTool::ChatGPTTool()
    : Tool("chatgpt", "Interact with OpenAI's ChatGPT API for text generation and conversation"),
      base_url_("https://api.openai.com/v1/chat/completions") {

    // Load API key from environment
    api_key_ = load_api_key_from_env();

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

std::string ChatGPTTool::load_api_key_from_env() const {
    const char* env_key = std::getenv("OPENAI_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    return "";
}

bool ChatGPTTool::has_api_key() const {
    return !api_key_.empty();
}

std::vector<ChatMessage> ChatGPTTool::extract_messages(const std::any& messages_param) const {
    std::vector<ChatMessage> messages;

    try {
        // Try to extract as json array
        if (messages_param.type() == typeid(json)) {
            json messages_json = std::any_cast<json>(messages_param);

            if (messages_json.is_array()) {
                for (const auto& msg : messages_json) {
                    if (msg.contains("role") && msg.contains("content")) {
                        messages.emplace_back(
                            msg["role"].get<std::string>(),
                            msg["content"].get<std::string>()
                        );
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to extract messages: " + std::string(e.what()));
    }

    return messages;
}

std::string ChatGPTTool::make_api_request(const json& request_body) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;
    std::string request_str = request_body.dump();

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key_;
    headers = curl_slist_append(headers, auth_header.c_str());

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, base_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    return response_data;
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

        std::vector<ChatMessage> messages = extract_messages(messages_it->second);

        // Add system prompt if provided
        auto system_it = params.find("system_prompt");
        if (system_it != params.end()) {
            std::string system_prompt = std::any_cast<std::string>(system_it->second);
            messages.insert(messages.begin(), ChatMessage("system", system_prompt));
        }

        if (messages.empty()) {
            return ToolResult::Error("No valid messages provided");
        }

        // Build request body
        json request_body;

        // Model
        auto model_it = params.find("model");
        request_body["model"] = (model_it != params.end())
            ? std::any_cast<std::string>(model_it->second)
            : "gpt-3.5-turbo";

        // Messages
        json messages_json = json::array();
        for (const auto& msg : messages) {
            messages_json.push_back(msg.to_json());
        }
        request_body["messages"] = messages_json;

        // Temperature
        auto temp_it = params.find("temperature");
        if (temp_it != params.end()) {
            double temp = std::any_cast<double>(temp_it->second);
            request_body["temperature"] = temp;
        }

        // Max tokens
        auto max_tokens_it = params.find("max_tokens");
        if (max_tokens_it != params.end()) {
            int max_tokens = std::any_cast<int>(max_tokens_it->second);
            request_body["max_tokens"] = max_tokens;
        }

        // Make API request
        std::string response_str = make_api_request(request_body);

        // Parse response
        json response = json::parse(response_str);

        // Check for errors
        if (response.contains("error")) {
            std::string error_msg = response["error"]["message"].get<std::string>();
            return ToolResult::Error("OpenAI API error: " + error_msg);
        }

        // Extract completion
        if (response.contains("choices") && !response["choices"].empty()) {
            std::string completion = response["choices"][0]["message"]["content"].get<std::string>();

            // Return full response as data for advanced usage
            return ToolResult::Success(response, completion);
        }

        return ToolResult::Error("No completion in response");

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
