#include "../include/GeminiCompletionEngine.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>

// Callback function for libcurl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

GeminiCompletionEngine::GeminiCompletionEngine()
    : api_key_(load_api_key_from_env()),
      base_url_("https://generativelanguage.googleapis.com/v1beta/models/") {
}

std::string GeminiCompletionEngine::load_api_key_from_env() const {
    const char* key = std::getenv("GEMINI_API_KEY");
    if (!key) {
        return "";
    }
    return std::string(key);
}

bool GeminiCompletionEngine::has_api_key() const {
    return !api_key_.empty();
}

std::pair<std::string, json> GeminiCompletionEngine::convert_messages_to_gemini_format(
    const std::vector<ChatMessage>& messages) const {

    std::string system_instruction;
    json gemini_messages = json::array();

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            // Gemini uses systemInstruction field for system messages
            if (!system_instruction.empty()) {
                system_instruction += "\n\n";
            }
            system_instruction += msg.content;
        } else if (msg.role == "user") {
            // User messages
            json gemini_msg = {
                {"role", "user"},
                {"parts", json::array({
                    {{"text", msg.content}}
                })}
            };
            gemini_messages.push_back(gemini_msg);
        } else if (msg.role == "assistant") {
            // Assistant messages (Gemini calls it "model")
            json parts = json::array();

            // Add text content if present
            if (!msg.content.empty()) {
                parts.push_back({{"text", msg.content}});
            }

            // Add function calls if present
            if (!msg.tool_calls.is_null() && msg.tool_calls.is_array()) {
                for (const auto& tool_call : msg.tool_calls) {
                    if (tool_call.contains("function")) {
                        json function_call = {
                            {"functionCall", {
                                {"name", tool_call["function"]["name"]},
                                {"args", tool_call["function"]["arguments"]}
                            }}
                        };
                        parts.push_back(function_call);
                    }
                }
            }

            json gemini_msg = {
                {"role", "model"},
                {"parts", parts}
            };
            gemini_messages.push_back(gemini_msg);
        } else if (msg.role == "tool") {
            // Tool response messages
            json gemini_msg = {
                {"role", "function"},
                {"parts", json::array({
                    {
                        {"functionResponse", {
                            {"name", msg.tool_call_id},
                            {"response", {
                                {"content", msg.content}
                            }}
                        }}
                    }
                })}
            };
            gemini_messages.push_back(gemini_msg);
        }
    }

    return {system_instruction, gemini_messages};
}

std::string GeminiCompletionEngine::make_request(const json& request_body) {
    if (!has_api_key()) {
        throw std::runtime_error("GEMINI_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;

    // Extract model from request body to construct URL
    std::string model = request_body.value("model", "gemini-1.5-flash");

    // Gemini API URL format:
    // https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={api_key}
    std::string url = base_url_ + model + ":generateContent?key=" + api_key_;

    // Build request body (excluding model since it's in the URL)
    json gemini_request;
    if (request_body.contains("contents")) {
        gemini_request["contents"] = request_body["contents"];
    }
    if (request_body.contains("systemInstruction")) {
        gemini_request["systemInstruction"] = request_body["systemInstruction"];
    }
    if (request_body.contains("generationConfig")) {
        gemini_request["generationConfig"] = request_body["generationConfig"];
    }
    if (request_body.contains("tools")) {
        gemini_request["tools"] = request_body["tools"];
    }

    std::string request_str = gemini_request.dump();

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

json GeminiCompletionEngine::build_request_body(const std::vector<ChatMessage>& messages,
                                                const std::string& model,
                                                double temperature,
                                                int max_tokens,
                                                ToolRegistryPtr registryPtr,
                                                const std::string& tool_choice) const {
    json request_body;

    // Model (stored in request_body for make_request to extract)
    request_body["model"] = model;

    // Truncate messages to fit context window
    std::vector<ChatMessage> truncated_messages = truncate_messages(messages, model, max_tokens);

    // Convert messages to Gemini format
    auto [system_instruction, gemini_messages] = convert_messages_to_gemini_format(truncated_messages);

    // Add system instruction if present
    if (!system_instruction.empty()) {
        request_body["systemInstruction"] = {
            {"parts", json::array({
                {{"text", system_instruction}}
            })}
        };
    }

    // Add messages (Gemini calls them "contents")
    request_body["contents"] = gemini_messages;

    // Generation config
    json generation_config;
    generation_config["temperature"] = temperature;

    if (max_tokens > 0) {
        generation_config["maxOutputTokens"] = max_tokens;
    }

    request_body["generationConfig"] = generation_config;

    // Add tools if registry is provided and has tools
    if (registryPtr != nullptr && registryPtr->size() > 0) {
        json tools_schema = registryPtr->get_all_schemas();

        // Convert OpenAI function format to Gemini function format
        json gemini_tools = json::array();
        json function_declarations = json::array();

        for (const auto& tool : tools_schema) {
            if (tool.contains("function")) {
                json gemini_function = {
                    {"name", tool["function"]["name"]},
                    {"description", tool["function"]["description"]},
                    {"parameters", tool["function"]["parameters"]}
                };
                function_declarations.push_back(gemini_function);
            }
        }

        gemini_tools.push_back({
            {"functionDeclarations", function_declarations}
        });

        request_body["tools"] = gemini_tools;
    }

    return request_body;
}
