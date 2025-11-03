#ifndef GITHUB_TOOL_HPP
#define GITHUB_TOOL_HPP

#include "Tool.hpp"
#include "ToolResult.hpp"
#include <string>

/**
 * @brief Tool for interacting with the GitHub API
 *
 * Allows agents to perform GitHub operations like reading files,
 * listing repositories, managing issues, and more.
 *
 * Currently supported operations:
 * - get_file: Get the contents of a file from a repository
 * - get_pull_requests: Get the pull requests of a repository
 */
class GitHubTool : public Tool {
private:
    std::string api_token_;
    std::string base_url_;

    /**
     * @brief Load GitHub token from GITHUB_TOKEN environment variable
     */
    std::string load_token_from_env() const;

    /**
     * @brief Make an HTTP request to the GitHub API
     *
     * @param endpoint API endpoint (e.g., "/repos/owner/repo/contents/path")
     * @param method HTTP method (GET, POST, PATCH, etc.)
     * @param body Request body as JSON (optional)
     * @return Response body as string
     */
    std::string make_github_request(const std::string& endpoint,
                                    const std::string& method = "GET",
                                    const json& body = json(nullptr)) const;

    /**
     * @brief Execute get_file operation
     *
     * Gets the contents of a file from a GitHub repository.
     *
     * Required params: owner, repo, path
     * Optional params: branch (default: main)
     */
    ToolResult execute_get_file(const std::map<std::string, std::any>& params);
 
    /**
     * @brief Execute get_pull_requests operation
     *
     * Gets the pull requests of a GitHub repository.
     *
     * Required params: owner, repo
     * Optional params: state (open, closed, all - default: open)
     */   
    ToolResult execute_get_pull_requests(const std::map<std::string, std::any>& params);


    // Helper methods for parameter extraction
    double get_number_param(const std::any& value) const;
    std::string get_string_param(const std::any& value) const;
    int get_int_param(const std::any& value) const;

public:
    GitHubTool();

    /**
     * @brief Execute GitHub operation
     *
     * Parameters:
     * - operation (string): The operation to perform ("get_file" or "get_pull_requests")
     * - owner (string): Repository owner (username or organization)
     * - repo (string): Repository name
     * - path (string): File path within the repository (required for get_file)
     * - branch (string, optional): Branch name (default: "main") (for get_file)
     * - state (string, optional): PR state filter (open, closed, all) (for get_pull_requests)
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

    /**
     * @brief Check if GitHub token is available
     */
    bool has_token() const { return !api_token_.empty(); }
};

#endif // GITHUB_TOOL_HPP
