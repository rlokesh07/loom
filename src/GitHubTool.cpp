#include "../include/GitHubTool.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>
#include <sstream>

// Callback function for libcurl to write response data
static size_t github_write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

GitHubTool::GitHubTool()
    : Tool("github", "Interact with GitHub API to read files, manage issues, and more"),
      base_url_("https://api.github.com") {

    api_token_ = load_token_from_env();

    // Define parameters
    add_parameter(Parameter("operation", ParameterType::STRING,
                           "The GitHub operation to perform (e.g., 'get_file', 'get_pull_requests', 'get_pull_request_details', 'get_pull_request_files')", true));

    add_parameter(Parameter("owner", ParameterType::STRING,
                           "Repository owner (username or organization)", true));

    add_parameter(Parameter("repo", ParameterType::STRING,
                           "Repository name", true));

    add_parameter(Parameter("path", ParameterType::STRING,
                           "File path within the repository", false));

    add_parameter(Parameter("branch", ParameterType::STRING,
                           "Branch name (default: main)", false));

    add_parameter(Parameter("pr", ParameterType::STRING,
                           "Pull request number for get_pull_request_details", false));
}

std::string GitHubTool::load_token_from_env() const {
    const char* token = std::getenv("GITHUB_TOKEN");
    if (token) {
        return std::string(token);
    }
    return "";
}

std::string GitHubTool::make_github_request(const std::string& endpoint,
                                            const std::string& method,
                                            const json& body) const {
    if (!has_token()) {
        throw std::runtime_error("GITHUB_TOKEN environment variable not set");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_data;
    std::string url = base_url_ + endpoint;

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: loom-github-tool");

    std::string auth_header = "Authorization: Bearer " + api_token_;
    headers = curl_slist_append(headers, auth_header.c_str());

    // Prepare request body if provided
    std::string request_body;
    if (!body.is_null()) {
        request_body = body.dump();
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, github_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Set HTTP method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!request_body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        }
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (!request_body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default

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
            std::string error_msg = "GitHub API error (HTTP " + std::to_string(http_code) + ")";
            if (error_response.contains("message")) {
                error_msg += ": " + error_response["message"].get<std::string>();
            }
            throw std::runtime_error(error_msg);
        } catch (const json::parse_error&) {
            throw std::runtime_error("GitHub API error (HTTP " + std::to_string(http_code) + ")");
        }
    }

    return response_data;
}

ToolResult GitHubTool::execute(const std::map<std::string, std::any>& params) {
    // Check for token
    if (!has_token()) {
        return ToolResult::Error("GITHUB_TOKEN environment variable not set");
    }

    try {
        // Extract operation
        auto operation_it = params.find("operation");
        if (operation_it == params.end()) {
            return ToolResult::Error("Missing required parameter: operation");
        }

        std::string operation = get_string_param(operation_it->second);

        // Route to appropriate operation handler
        if (operation == "get_file") {
            return execute_get_file(params);
        } else if (operation == "get_pull_requests") {
            return execute_get_pull_requests(params);
        } else if (operation == "get_pull_request_details") {
            return execute_get_pull_request_details(params);
        } else if (operation == "get_pull_request_files") {
            return execute_get_pull_request_files(params);
        } else {
            return ToolResult::Error("Unknown operation: " + operation);
        }

    } catch (const std::bad_any_cast& e) {
        return ToolResult::Error("Type conversion error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Execution error: " + std::string(e.what()));
    }
}
ToolResult GitHubTool::execute_get_pull_request_files(const std::map<std::string, std::any>& params) {
    try {
        // Extract required parameters
        auto owner_it = params.find("owner");
        auto repo_it = params.find("repo");
        auto pr_it = params.find("pr");

        if (owner_it == params.end() || repo_it == params.end() || pr_it == params.end()) {
            return ToolResult::Error("Missing required parameters: owner, repo, and pr are required for get_pull_request_files");
        }

        std::string owner = get_string_param(owner_it->second);
        std::string repo = get_string_param(repo_it->second);
        std::string pr = get_string_param(pr_it->second);
        // Build endpoint
        std::string endpoint = "/repos/" + owner + "/" + repo + "/pulls/" + pr + "/files";

        // Make API request
        std::string response_str = make_github_request(endpoint);

        // Parse response
        json response = json::parse(response_str);

        // GitHub returns an array of file objects
        if (response.is_array()) {
            // Build result message
            std::ostringstream result_msg;
            result_msg << "Found " << response.size() << " files changed in PR #" << pr << " for " << owner << "/" << repo << ":\n\n";
            
            for (const auto& file : response) {
                std::string filename = file["filename"].get<std::string>();
                std::string status = file["status"].get<std::string>();
                int additions = file["additions"].get<int>();
                int deletions = file["deletions"].get<int>();
                int changes = file["changes"].get<int>();
                
                result_msg << "📄 " << filename << "\n";
                result_msg << "   Status: " << status << "\n";
                result_msg << "   Changes: +" << additions << " -" << deletions << " (~" << changes << ")\n";
                
                if (file.contains("patch") && !file["patch"].is_null()) {
                    std::string patch = file["patch"].get<std::string>();
                    result_msg << "   Patch preview:\n";
                    // Show first few lines of patch
                    std::istringstream patch_stream(patch);
                    std::string line;
                    int line_count = 0;
                    while (std::getline(patch_stream, line) && line_count < 5) {
                        result_msg << "   " << line << "\n";
                        line_count++;
                    }
                    if (line_count == 5) {
                        result_msg << "   ...\n";
                    }
                }
                result_msg << "\n";
            }

            // Return success with full response data
            return ToolResult::Success(response, result_msg.str());
        } else {
            return ToolResult::Error("Unexpected response format from GitHub API");
        }

    } catch (const json::parse_error& e) {
        return ToolResult::Error("Failed to parse GitHub API response: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Error getting pull request files: " + std::string(e.what()));
    }
}


ToolResult GitHubTool::execute_get_pull_request_details(const std::map<std::string, std::any>& params) {
    try {
        // Extract required parameters
        auto owner_it = params.find("owner");
        auto repo_it = params.find("repo");
        auto pr_it = params.find("pr");

        if (owner_it == params.end() || repo_it == params.end() || pr_it == params.end()) {
            return ToolResult::Error("Missing required parameters: owner, repo, and pr are required for get_pull_request_details");
        }

        std::string owner = get_string_param(owner_it->second);
        std::string repo = get_string_param(repo_it->second);
        std::string pr = get_string_param(pr_it->second);
        // Build endpoint
        std::string endpoint = "/repos/" + owner + "/" + repo + "/pulls/" + pr;

        // Make API request
        std::string response_str = make_github_request(endpoint);

        // Parse response
        json response = json::parse(response_str);

        // GitHub returns a single pull request object
        if (response.is_object()) {
            // Build result message
            std::ostringstream result_msg;
            
            int number = response["number"].get<int>();
            std::string title = response["title"].get<std::string>();
            std::string state = response["state"].get<std::string>();
            std::string user = response["user"]["login"].get<std::string>();
            std::string body = response["body"].is_null() ? "" : response["body"].get<std::string>();
            std::string created_at = response["created_at"].get<std::string>();
            std::string updated_at = response["updated_at"].get<std::string>();
            
            result_msg << "Pull Request #" << number << ": " << title << "\n";
            result_msg << "State: " << state << "\n";
            result_msg << "Author: " << user << "\n";
            result_msg << "Created: " << created_at << "\n";
            result_msg << "Updated: " << updated_at << "\n";
            if (!body.empty()) {
                result_msg << "Description:\n" << body << "\n";
            }

            // Return success with full response data
            return ToolResult::Success(response, result_msg.str());
        } else {
            return ToolResult::Error("Unexpected response format from GitHub API");
        }

    } catch (const json::parse_error& e) {
        return ToolResult::Error("Failed to parse GitHub API response: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Error getting pull request details: " + std::string(e.what()));
    }
}


ToolResult GitHubTool::execute_get_pull_requests(const std::map<std::string, std::any>& params) {
    try {
        // Extract required parameters
        auto owner_it = params.find("owner");
        auto repo_it = params.find("repo");

        if (owner_it == params.end() || repo_it == params.end()) {
            return ToolResult::Error("Missing required parameters: owner and repo are required for get_pull_requests");
        }

        std::string owner = get_string_param(owner_it->second);
        std::string repo = get_string_param(repo_it->second);

        // Build endpoint
        std::string endpoint = "/repos/" + owner + "/" + repo + "/pulls";

        // Make API request
        std::string response_str = make_github_request(endpoint);

        // Parse response
        json response = json::parse(response_str);

        // GitHub returns an array of pull request objects
        if (response.is_array()) {
            // Build result message
            std::ostringstream result_msg;
            result_msg << "Found " << response.size() << " pull requests for " << owner << "/" << repo << ":\n";
            
            for (const auto& pr : response) {
                int number = pr["number"].get<int>();
                std::string title = pr["title"].get<std::string>();
                std::string state = pr["state"].get<std::string>();
                std::string user = pr["user"]["login"].get<std::string>();
                
                result_msg << "#" << number << ": " << title << " (" << state << ") by " << user << "\n";
            }

            // Return success with full response data
            return ToolResult::Success(response, result_msg.str());
        } else {
            return ToolResult::Error("Unexpected response format from GitHub API");
        }

    } catch (const json::parse_error& e) {
        return ToolResult::Error("Failed to parse GitHub API response: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Error getting pull requests: " + std::string(e.what()));
    }
}

ToolResult GitHubTool::execute_get_file(const std::map<std::string, std::any>& params) {
    try {
        // Extract required parameters
        auto owner_it = params.find("owner");
        auto repo_it = params.find("repo");
        auto path_it = params.find("path");

        if (owner_it == params.end() || repo_it == params.end() || path_it == params.end()) {
            return ToolResult::Error("Missing required parameters: owner, repo, and path are required for get_file");
        }

        std::string owner = get_string_param(owner_it->second);
        std::string repo = get_string_param(repo_it->second);
        std::string path = get_string_param(path_it->second);

        // Optional: branch parameter
        std::string branch = "main";
        auto branch_it = params.find("branch");
        if (branch_it != params.end()) {
            branch = get_string_param(branch_it->second);
        }

        // Build endpoint
        std::string endpoint = "/repos/" + owner + "/" + repo + "/contents/" + path + "?ref=" + branch;

        // Make API request
        std::string response_str = make_github_request(endpoint);

        // Parse response
        json response = json::parse(response_str);

        // GitHub returns base64-encoded content for files
        if (response.contains("content") && response.contains("encoding")) {
            std::string encoding = response["encoding"].get<std::string>();
            std::string content = response["content"].get<std::string>();
            std::string name = response["name"].get<std::string>();
            int size = response["size"].get<int>();

            // Build result message
            std::ostringstream result_msg;
            result_msg << "File: " << name << " (size: " << size << " bytes)\n";
            result_msg << "Encoding: " << encoding << "\n";
            result_msg << "Path: " << path << "\n";
            result_msg << "Branch: " << branch;

            // Return success with full response data
            return ToolResult::Success(response, result_msg.str());
        } else {
            return ToolResult::Error("Unexpected response format from GitHub API");
        }

    } catch (const json::parse_error& e) {
        return ToolResult::Error("Failed to parse GitHub API response: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return ToolResult::Error("Error getting file: " + std::string(e.what()));
    }
}

// Helper methods for parameter extraction
double GitHubTool::get_number_param(const std::any& value) const {
    if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    } else if (value.type() == typeid(int)) {
        return static_cast<double>(std::any_cast<int>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<double>(std::any_cast<float>(value));
    }
    throw std::bad_any_cast();
}

std::string GitHubTool::get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}

int GitHubTool::get_int_param(const std::any& value) const {
    if (value.type() == typeid(int)) {
        return std::any_cast<int>(value);
    } else if (value.type() == typeid(double)) {
        return static_cast<int>(std::any_cast<double>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<int>(std::any_cast<float>(value));
    }
    throw std::bad_any_cast();
}
