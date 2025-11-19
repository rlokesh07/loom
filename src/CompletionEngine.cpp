#include "../include/CompletionEngine.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::vector<ChatMessage> CompletionEngine::extract_messages(const std::any& messages_param) const {
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
std::string CompletionEngine::parse_completion_response(const std::string& response_str,
                                                               json& out_full_response) const {
    // Parse response
    json response = json::parse(response_str);
    out_full_response = response;

    // Check for errors
    if (response.contains("error")) {
        std::string error_msg = response["error"]["message"].get<std::string>();
        throw std::runtime_error("OpenAI API error: " + error_msg);
    }

    // Extract completion
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& message = response["choices"][0]["message"];

        // Check if content exists and is not null
        if (message.contains("content") && !message["content"].is_null()) {
            return message["content"].get<std::string>();
        }

        // If no content but there are tool calls, return empty string
        // (tool calls will be handled separately)
        if (message.contains("tool_calls")) {
            return "";
        }
    }

    throw std::runtime_error("No completion in response");
}
