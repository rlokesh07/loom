#include "../include/CompletionEngine.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <iostream>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Model context window limits (in tokens)
const std::map<std::string, int> CompletionEngine::MODEL_CONTEXT_LIMITS = {
    // OpenAI models
    {"gpt-3.5-turbo", 4096},
    {"gpt-3.5-turbo-16k", 16384},
    {"gpt-4", 8192},
    {"gpt-4-32k", 32768},
    {"gpt-4-turbo", 128000},
    {"gpt-4-turbo-preview", 128000},
    {"gpt-4o", 128000},
    {"gpt-4o-mini", 128000},

    // Google Gemini models
    {"gemini-1.0-pro", 32768},
    {"gemini-1.5-pro", 2097152},      // 2M tokens
    {"gemini-1.5-flash", 1048576},    // 1M tokens
    {"gemini-2.0-flash", 1048576},    // 1M tokens
    {"gemini-pro", 32768},

    // Ollama/Open source models
    {"llama2", 4096},
    {"llama3", 8192},
    {"llama3.1", 128000},
    {"llama3.2", 8192},
    {"mistral", 8192},
    {"mixtral", 32768},
    {"codellama", 16384},

    // Default fallback
    {"default", 4096}
};

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
                                                               json& out_full_response,
                                                               TokenUsage* out_usage) const {
    // Parse response
    json response = json::parse(response_str);
    out_full_response = response;

    // Extract token usage if requested and available
    if (out_usage && response.contains("usage")) {
        const auto& usage = response["usage"];
        if (usage.contains("prompt_tokens")) {
            out_usage->prompt_tokens = usage["prompt_tokens"].get<int>();
        }
        if (usage.contains("completion_tokens")) {
            out_usage->completion_tokens = usage["completion_tokens"].get<int>();
        }
        if (usage.contains("total_tokens")) {
            out_usage->total_tokens = usage["total_tokens"].get<int>();
        }
    }

    // Check for errors in error field
    if (response.contains("error")) {
        std::string error_msg = response["error"]["message"].get<std::string>();
        std::cerr << "OpenAI API Error: " << error_msg << std::endl;
        std::cerr << "Full response: " << response.dump(2) << std::endl;
        throw std::runtime_error("OpenAI API error: " + error_msg);
    }

    // Extract completion
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& choice = response["choices"][0];

        // Check for error finish_reason
        if (choice.contains("finish_reason") && choice["finish_reason"] == "error") {
            std::string error_msg = "OpenAI returned finish_reason='error'";
            std::cerr << error_msg << std::endl;
            std::cerr << "Full response: " << response.dump(2) << std::endl;

            // Try to extract more error details if available
            if (response.contains("error")) {
                error_msg += ": " + response["error"]["message"].get<std::string>();
            }
            throw std::runtime_error(error_msg);
        }

        const auto& message = choice["message"];

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

    std::cerr << "No completion in response: " << response.dump(2) << std::endl;
    throw std::runtime_error("No completion in response");
}

std::vector<ChatMessage> CompletionEngine::truncate_messages(
    const std::vector<ChatMessage>& messages,
    const std::string& model,
    int max_completion_tokens) {

    // Get model's context window limit
    int context_limit = 4096; // Default
    auto it = MODEL_CONTEXT_LIMITS.find(model);
    if (it != MODEL_CONTEXT_LIMITS.end()) {
        context_limit = it->second;
    } else {
        // Try partial match for model variants (e.g., "gpt-4-0613" matches "gpt-4")
        for (const auto& [key, limit] : MODEL_CONTEXT_LIMITS) {
            if (model.find(key) == 0) {
                context_limit = limit;
                break;
            }
        }
    }

    // Reserve tokens for completion (default 1024 if not specified)
    int reserved_tokens = (max_completion_tokens > 0) ? max_completion_tokens : 1024;

    // Calculate available tokens for input messages
    int available_tokens = context_limit - reserved_tokens;

    // If no messages, return empty
    if (messages.empty()) {
        return messages;
    }

    // Helper to estimate tokens (1 token ≈ 4 chars + 20% buffer)
    auto estimate_tokens = [](const std::string& text) -> int {
        return static_cast<int>((text.length() / 4.0) * 1.2);
    };

    // Helper to count message tokens
    auto count_message_tokens = [&estimate_tokens](const ChatMessage& msg) -> int {
        int tokens = estimate_tokens(msg.content);
        tokens += 4; // Message structure overhead

        // Add tokens for tool calls if present
        if (!msg.tool_calls.is_null() && msg.tool_calls.is_array()) {
            tokens += estimate_tokens(msg.tool_calls.dump());
        }

        return tokens;
    };

    // Strategy: Keep system messages + most recent messages that fit
    std::vector<ChatMessage> truncated;
    int token_count = 0;

    // Step 1: Add all system messages first (always keep these)
    for (const auto& msg : messages) {
        if (msg.role == "system") {
            truncated.push_back(msg);
            token_count += count_message_tokens(msg);
        }
    }

    // Step 2: Collect non-system messages
    std::vector<ChatMessage> non_system_messages;
    for (const auto& msg : messages) {
        if (msg.role != "system") {
            non_system_messages.push_back(msg);
        }
    }

    // Step 3: Add messages from most recent backwards until we hit limit
    for (auto it = non_system_messages.rbegin(); it != non_system_messages.rend(); ++it) {
        int msg_tokens = count_message_tokens(*it);

        // Check if adding this message would exceed limit
        if (token_count + msg_tokens > available_tokens) {
            // Log warning if we're dropping messages
            if (truncated.size() < messages.size()) {
                std::cerr << "WARNING: Context window truncation for model '" << model
                          << "' - keeping " << truncated.size() << " of " << messages.size()
                          << " messages (limit: " << context_limit << " tokens)\n";
            }
            break;
        }

        truncated.push_back(*it);
        token_count += msg_tokens;
    }

    // Step 4: Reverse the non-system messages to restore chronological order
    size_t system_count = 0;
    for (const auto& msg : truncated) {
        if (msg.role == "system") system_count++;
    }

    // Reverse only the non-system portion
    if (truncated.size() > system_count) {
        std::reverse(truncated.begin() + system_count, truncated.end());
    }

    return truncated;
}
