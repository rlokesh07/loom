#ifndef WEBSEARCH_TOOL_HPP
#define WEBSEARCH_TOOL_HPP

#include "Tool.hpp"
#include "ToolResult.hpp"
#include <string>

/**
 * @brief Tool for performing web searches using the Brave Search API
 *
 * Allows agents to search the web for current information and retrieve
 * search results with titles, snippets, and URLs.
 *
 * Requires BRAVE_SEARCH_API_KEY environment variable to be set.
 * Get your API key from: https://api.search.brave.com/app/keys
 *
 * Free tier: 2,000 queries/month
 */
class WebSearchTool : public Tool {
private:
    std::string api_key_;
    std::string base_url_;

    /**
     * @brief Load Brave Search API key from BRAVE_SEARCH_API_KEY environment variable
     */
    std::string load_api_key_from_env() const;

    /**
     * @brief Make an HTTP request to the Brave Search API
     *
     * @param query Search query string
     * @param count Number of results to return (1-20)
     * @return Response body as string (JSON)
     */
    std::string make_search_request(const std::string& query, int count = 10) const;

    // Helper methods for parameter extraction
    std::string get_string_param(const std::any& value) const;
    int get_int_param(const std::any& value) const;

public:
    WebSearchTool();

    /**
     * @brief Execute web search
     *
     * Parameters:
     * - query (string, required): The search query
     * - count (integer, optional): Number of results to return (1-20, default: 10)
     *
     * Returns:
     * JSON object with search results containing:
     * - query: The original search query
     * - results: Array of result objects with title, url, description
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

    /**
     * @brief Check if API key is available
     */
    bool has_api_key() const { return !api_key_.empty(); }
};

#endif // WEBSEARCH_TOOL_HPP
