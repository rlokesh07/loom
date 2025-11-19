#ifndef WEBFETCH_TOOL_HPP
#define WEBFETCH_TOOL_HPP

#include "Tool.hpp"
#include "ToolResult.hpp"
#include <string>

/**
 * @brief Tool for fetching and reading content from web URLs
 *
 * Allows agents to retrieve the content of web pages by URL.
 * Returns the text content of the page with HTML tags removed
 * for easier consumption by LLMs.
 *
 * No API key required - uses direct HTTP GET requests.
 */
class WebFetchTool : public Tool {
private:
    /**
     * @brief Make an HTTP GET request to fetch URL content
     *
     * @param url The URL to fetch
     * @return Response body as string (HTML)
     */
    std::string fetch_url(const std::string& url) const;

    /**
     * @brief Basic HTML to text conversion
     *
     * Removes HTML tags and cleans up whitespace for better readability
     *
     * @param html Raw HTML content
     * @return Plain text content
     */
    std::string html_to_text(const std::string& html) const;

    /**
     * @brief Validate URL format
     *
     * @param url URL to validate
     * @return true if URL is valid, false otherwise
     */
    bool is_valid_url(const std::string& url) const;

    // Helper methods for parameter extraction
    std::string get_string_param(const std::any& value) const;
    int get_int_param(const std::any& value) const;

public:
    WebFetchTool();

    /**
     * @brief Fetch and read content from a URL
     *
     * Parameters:
     * - url (string, required): The URL to fetch
     * - max_length (integer, optional): Maximum content length in characters (default: 10000)
     *
     * Returns:
     * JSON object with:
     * - url: The fetched URL
     * - content: Plain text content of the page
     * - length: Content length in characters
     * - truncated: Whether content was truncated
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;
};

#endif // WEBFETCH_TOOL_HPP
