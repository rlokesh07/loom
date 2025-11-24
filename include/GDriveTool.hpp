#ifndef GDRIVE_TOOL_HPP
#define GDRIVE_TOOL_HPP

#include "Tool.hpp"
#include "ToolResult.hpp"
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Tool for interacting with Google Drive
 *
 * Allows agents to list, search, read, create, move, and delete files
 * and folders in Google Drive.
 *
 * Requires GOOGLE_OAUTH_CREDENTIALS environment variable to be set
 * to the path of a JSON file containing:
 * {
 *   "client_id": "xxx.apps.googleusercontent.com",
 *   "client_secret": "xxx",
 *   "refresh_token": "xxx"
 * }
 */
class GDriveTool : public Tool {
private:
    std::string oauth_credentials_path_;
    std::string access_token_;
    std::chrono::time_point<std::chrono::system_clock> token_expiry_;

    /**
     * @brief Load OAuth credentials path from environment
     */
    std::string load_credentials_path_from_env() const;

    /**
     * @brief Authenticate using refresh token
     */
    bool authenticate();

    /**
     * @brief Check if current token is valid
     */
    bool is_token_valid() const;

    /**
     * @brief Make HTTP request to Google Drive API
     */
    std::string make_api_request(const std::string& endpoint,
                                 const std::string& method = "GET",
                                 const std::string& body = "",
                                 const std::string& content_type = "application/json") const;

    // Operations
    ToolResult list_files(const std::string& folder_id, int max_results);
    ToolResult search_files(const std::string& query, int max_results);
    ToolResult read_file(const std::string& file_id);
    ToolResult create_file(const std::string& name, const std::string& content,
                          const std::string& mime_type, const std::string& folder_id);
    ToolResult create_folder(const std::string& name, const std::string& parent_folder_id);
    ToolResult upload_content(const std::string& file_id, const std::string& content);
    ToolResult move_file(const std::string& file_id, const std::string& destination_folder_id);
    ToolResult delete_file(const std::string& file_id);
    ToolResult get_file_info(const std::string& file_id);

    /**
     * @brief Export Google Doc/Sheet to text
     */
    std::string export_google_doc(const std::string& file_id, const std::string& mime_type) const;

    // Helper methods
    std::string get_string_param(const std::any& value) const;
    int get_int_param(const std::any& value) const;

public:
    GDriveTool();

    /**
     * @brief Execute Google Drive operation
     *
     * Parameters:
     * - operation (string, required): list, search, read, create, create_folder,
     *                                  upload, move, delete, get_info
     * - file_id (string): File/folder ID for operations that need it
     * - folder_id (string): Parent folder ID (default: root)
     * - destination_folder_id (string): For move operation
     * - query (string): Search query
     * - name (string): File/folder name for create
     * - content (string): File content for create/upload
     * - mime_type (string): MIME type for create (default: text/plain)
     * - max_results (int): Max results for list/search (default: 20)
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

    /**
     * @brief Check if OAuth credentials are available
     */
    bool has_credentials() const { return !oauth_credentials_path_.empty(); }
};

#endif // GDRIVE_TOOL_HPP
