#include "../include/OllamaCompletionEngine.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

OllamaCompletionEngine::OllamaCompletionEngine()
    : base_url_("http://localhost:11434/v1/chat/completions") {
    // Check if custom Ollama URL is set
    const char* env_url = std::getenv("OLLAMA_BASE_URL");
    if (env_url) {
        base_url_ = std::string(env_url);
    }
}

OllamaCompletionEngine::OllamaCompletionEngine(const std::string& base_url)
    : base_url_(base_url) {
}

std::string OllamaCompletionEngine::make_request(const json& request_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;
    std::string request_str = request_body.dump();

    // Set up headers - no API key needed for Ollama
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, base_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);  // Longer timeout for local models

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

json OllamaCompletionEngine::build_request_body(const std::vector<ChatMessage>& messages,
                                                 const std::string& model,
                                                 double temperature,
                                                 int max_tokens,
                                                 const json& tools,
                                                 const std::string& tool_choice) const {
    json request_body;

    // Model
    request_body["model"] = model;

    // Messages
    json messages_json = json::array();
    for (const auto& msg : messages) {
        messages_json.push_back(msg.to_json());
    }
    request_body["messages"] = messages_json;

    // Temperature
    request_body["temperature"] = temperature;

    // Max tokens (Ollama uses num_predict, but also supports max_tokens for OpenAI compatibility)
    if (max_tokens > 0) {
        request_body["max_tokens"] = max_tokens;
    }

    // Add tools if provided (Ollama supports OpenAI-compatible function calling)
    if (!tools.is_null() && !tools.empty()) {
        request_body["tools"] = tools;
        request_body["tool_choice"] = tool_choice;
    }

    // Disable streaming by default (can be made configurable later)
    request_body["stream"] = false;

    return request_body;
}
