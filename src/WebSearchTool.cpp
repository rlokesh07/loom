#include "../include/WebSearchTool.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>
#include <sstream>

// Callback function for libcurl to write response data
static size_t websearch_write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

WebSearchTool::WebSearchTool()
    : Tool("web_search", "Search the web using Brave Search API to find current information"),
      base_url_("https://api.search.brave.com/res/v1/web/search") {

    api_key_ = load_api_key_from_env();

    // Define parameters
    add_parameter(Parameter("query", ParameterType::STRING,
                           "The search query to execute", true));

    add_parameter(Parameter("count", ParameterType::INTEGER,
                           "Number of search results to return (1-20, default: 10)", false));
}

std::string WebSearchTool::load_api_key_from_env() const {
    const char* key = std::getenv("BRAVE_SEARCH_API_KEY");
    if (key) {
        return std::string(key);
    }
    return "";
}

std::string WebSearchTool::make_search_request(const std::string& query, int count) const {
    if (!has_api_key()) {
        throw std::runtime_error("BRAVE_SEARCH_API_KEY environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;

    // URL encode the query
    char* encoded_query = curl_easy_escape(curl, query.c_str(), query.length());
    if (!encoded_query) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to encode query");
    }

    // Build URL with query parameters
    std::string url = base_url_ + "?q=" + std::string(encoded_query) +
                      "&count=" + std::to_string(count);
    curl_free(encoded_query);

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string api_key_header = "X-Subscription-Token: " + api_key_;
    headers = curl_slist_append(headers, api_key_header.c_str());

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, websearch_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP response code
    if (http_code >= 400) {
        json error_response;
        try {
            error_response = json::parse(response_data);
            std::string error_msg = "Brave Search API error (HTTP " + std::to_string(http_code) + ")";
            if (error_response.contains("message")) {
                error_msg += ": " + error_response["message"].get<std::string>();
            }
            throw std::runtime_error(error_msg);
        } catch (const json::parse_error&) {
            throw std::runtime_error("Brave Search API error (HTTP " + std::to_string(http_code) + "): " + response_data);
        }
    }

    return response_data;
}

ToolResult WebSearchTool::execute(const std::map<std::string, std::any>& params) {
    if (!has_api_key()) {
        return ToolResult::Error("BRAVE_SEARCH_API_KEY environment variable not set. Get your API key from https://api.search.brave.com/app/keys");
    }

    try {
        // Extract query parameter
        auto query_it = params.find("query");
        if (query_it == params.end()) {
            return ToolResult::Error("Missing required parameter: query");
        }

        std::string query = get_string_param(query_it->second);

        if (query.empty()) {
            return ToolResult::Error("Query parameter cannot be empty");
        }

        // Extract optional count parameter
        int count = 10; // default
        auto count_it = params.find("count");
        if (count_it != params.end()) {
            count = get_int_param(count_it->second);

            // Validate count range
            if (count < 1 || count > 20) {
                return ToolResult::Error("Count parameter must be between 1 and 20");
            }
        }

        std::string response_str = make_search_request(query, count);

        json response = json::parse(response_str);

        // Extract web results
        if (!response.contains("web")) {
            return ToolResult::Error("No web results found in API response");
        }

        json web_results = response["web"];

        if (!web_results.contains("results") || !web_results["results"].is_array()) {
            return ToolResult::Error("Invalid response format from Brave Search API");
        }

        json results_array = web_results["results"];

        // Build formatted result message
        std::ostringstream result_msg;
        result_msg << "Found " << results_array.size() << " search results for query: \"" << query << "\"\n\n";

        // Build structured result data
        json structured_results = json::object();
        structured_results["query"] = query;
        structured_results["count"] = results_array.size();
        structured_results["results"] = json::array();

        int result_num = 1;
        for (const auto& result : results_array) {
            std::string title = result.contains("title") ? result["title"].get<std::string>() : "No title";
            std::string url = result.contains("url") ? result["url"].get<std::string>() : "No URL";
            std::string description = result.contains("description") ? result["description"].get<std::string>() : "No description";

            // Add to formatted message
            result_msg << result_num << ". " << title << "\n";
            result_msg << "   URL: " << url << "\n";
            result_msg << "   " << description << "\n\n";

            // Add to structured results
            json result_obj = json::object();
            result_obj["title"] = title;
            result_obj["url"] = url;
            result_obj["description"] = description;

            // Include additional metadata if available
            if (result.contains("age")) {
                result_obj["age"] = result["age"].get<std::string>();
            }
            if (result.contains("language")) {
                result_obj["language"] = result["language"].get<std::string>();
            }

            structured_results["results"].push_back(result_obj);
            result_num++;
        }

        // Return success with structured data
        return ToolResult::Success(structured_results, result_msg.str());

    } catch (const json::parse_error& e) {
        return ToolResult::Error("Failed to parse Brave Search API response: " + std::string(e.what()));
    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}

// Helper methods for parameter extraction
std::string WebSearchTool::get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}

int WebSearchTool::get_int_param(const std::any& value) const {
    if (value.type() == typeid(int)) {
        return std::any_cast<int>(value);
    } else if (value.type() == typeid(double)) {
        return static_cast<int>(std::any_cast<double>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<int>(std::any_cast<float>(value));
    } else if (value.type() == typeid(std::string)) {
        // Handle string inputs from JSON (LLMs sometimes send numbers as strings)
        std::string str = std::any_cast<std::string>(value);
        try {
            return std::stoi(str);
        } catch (const std::exception&) {
            throw std::bad_any_cast();
        }
    }
    throw std::bad_any_cast();
}
