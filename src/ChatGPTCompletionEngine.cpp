#include "../include/ChatGPTCompletionEngine.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

ChatGPTCompletionEngine::ChatGPTCompletionEngine()
    : base_url_("https://api.openai.com/v1/chat/completions") {
    api_key_ = load_api_key_from_env();
}

std::string ChatGPTCompletionEngine::load_api_key_from_env() const {
    const char* env_key = std::getenv("OPENAI_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    return "";
}

bool ChatGPTCompletionEngine::has_api_key() const {
    return !api_key_.empty();
}

std::string ChatGPTCompletionEngine::make_request(const json& request_body) {
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

json ChatGPTCompletionEngine::build_request_body(const std::vector<ChatMessage>& messages,
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

    // Max tokens (only add if specified)
    if (max_tokens > 0) {
        request_body["max_tokens"] = max_tokens;
    }

    // Add tools if provided
    if (!tools.is_null() && !tools.empty()) {
        request_body["tools"] = tools;
        request_body["tool_choice"] = tool_choice;
    }

    return request_body;
}
