#include "../include/WebFetchTool.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <algorithm>

// Callback function for libcurl to write response data
static size_t webfetch_write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

WebFetchTool::WebFetchTool()
    : Tool("web_fetch", "Fetch and read content from a web URL") {

    // Define parameters
    add_parameter(Parameter("url", ParameterType::STRING,
                           "The URL to fetch and read", true));

    add_parameter(Parameter("max_length", ParameterType::INTEGER,
                           "Maximum content length in characters (default: 10000)", false));
}

bool WebFetchTool::is_valid_url(const std::string& url) const {
    // Basic URL validation - must start with http:// or https://
    return url.find("http://") == 0 || url.find("https://") == 0;
}

std::string WebFetchTool::fetch_url(const std::string& url) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (compatible; LoomBot/1.0)");
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, webfetch_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("Failed to fetch URL: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP response code
    if (http_code >= 400) {
        throw std::runtime_error("HTTP error " + std::to_string(http_code) + " when fetching URL");
    }

    return response_data;
}

std::string WebFetchTool::html_to_text(const std::string& html) const {
    std::string text = html;

    // Remove script and style tags with their content
    std::regex script_style_regex("<(script|style)[^>]*>.*?</\\1>", std::regex::icase | std::regex::ECMAScript);
    text = std::regex_replace(text, script_style_regex, " ");

    // Remove HTML comments
    std::regex comment_regex("<!--.*?-->", std::regex::ECMAScript);
    text = std::regex_replace(text, comment_regex, " ");

    // Replace common block-level tags with newlines
    std::regex block_tags_regex("</(p|div|h[1-6]|li|tr|br)>", std::regex::icase);
    text = std::regex_replace(text, block_tags_regex, "\n");

    // Remove all remaining HTML tags
    std::regex html_tag_regex("<[^>]*>");
    text = std::regex_replace(text, html_tag_regex, " ");

    // Decode common HTML entities
    std::regex nbsp_regex("&nbsp;");
    text = std::regex_replace(text, nbsp_regex, " ");

    std::regex lt_regex("&lt;");
    text = std::regex_replace(text, lt_regex, "<");

    std::regex gt_regex("&gt;");
    text = std::regex_replace(text, gt_regex, ">");

    std::regex amp_regex("&amp;");
    text = std::regex_replace(text, amp_regex, "&");

    std::regex quot_regex("&quot;");
    text = std::regex_replace(text, quot_regex, "\"");

    // Clean up whitespace
    // Replace multiple spaces with single space
    std::regex multi_space_regex("[ \\t]+");
    text = std::regex_replace(text, multi_space_regex, " ");

    // Replace multiple newlines with double newline (paragraph break)
    std::regex multi_newline_regex("\\n\\s*\\n+");
    text = std::regex_replace(text, multi_newline_regex, "\n\n");

    // Trim leading/trailing whitespace from each line
    std::istringstream iss(text);
    std::ostringstream oss;
    std::string line;
    bool first = true;

    while (std::getline(iss, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        size_t end = line.find_last_not_of(" \t");

        if (start != std::string::npos && end != std::string::npos) {
            if (!first) oss << "\n";
            oss << line.substr(start, end - start + 1);
            first = false;
        }
    }

    return oss.str();
}

ToolResult WebFetchTool::execute(const std::map<std::string, std::any>& params) {
    try {
        // Extract required URL parameter
        auto url_it = params.find("url");
        if (url_it == params.end()) {
            return ToolResult::Error("Missing required parameter: url");
        }

        std::string url = get_string_param(url_it->second);

        // Validate URL
        if (url.empty()) {
            return ToolResult::Error("URL parameter cannot be empty");
        }

        if (!is_valid_url(url)) {
            return ToolResult::Error("Invalid URL format. URL must start with http:// or https://");
        }

        // Extract optional max_length parameter
        int max_length = 10000;  // default
        auto max_length_it = params.find("max_length");
        if (max_length_it != params.end()) {
            max_length = get_int_param(max_length_it->second);

            // Validate max_length range
            if (max_length < 100 || max_length > 100000) {
                return ToolResult::Error("max_length parameter must be between 100 and 100000");
            }
        }

        // Fetch URL content
        std::string html = fetch_url(url);

        // Convert HTML to plain text
        std::string text = html_to_text(html);

        // Truncate if needed
        bool truncated = false;
        if (static_cast<int>(text.length()) > max_length) {
            text = text.substr(0, max_length);
            truncated = true;
        }

        // Build result
        json result_data = json::object();
        result_data["url"] = url;
        result_data["content"] = text;
        result_data["length"] = text.length();
        result_data["truncated"] = truncated;

        // Build message
        std::ostringstream result_msg;
        result_msg << "Fetched content from: " << url << "\n";
        result_msg << "Content length: " << text.length() << " characters";
        if (truncated) {
            result_msg << " (truncated from original)";
        }
        result_msg << "\n\n" << text;

        return ToolResult::Success(result_data, result_msg.str());

    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}

// Helper methods for parameter extraction
std::string WebFetchTool::get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}

int WebFetchTool::get_int_param(const std::any& value) const {
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
