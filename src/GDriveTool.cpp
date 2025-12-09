#include "../include/GDriveTool.hpp"
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>

// Callback for CURL to write response
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

GDriveTool::GDriveTool()
    : OAuthTool("gdrive", "Interact with Google Drive files and folders", "google") {

    // Define parameters
    add_parameter(Parameter("operation", ParameterType::STRING,
        "Operation: list, search, read, create, create_folder, upload, move, delete, get_info", true));
    add_parameter(Parameter("file_id", ParameterType::STRING,
        "File or folder ID", false));
    add_parameter(Parameter("folder_id", ParameterType::STRING,
        "Parent folder ID (default: root)", false));
    add_parameter(Parameter("destination_folder_id", ParameterType::STRING,
        "Destination folder ID for move operation", false));
    add_parameter(Parameter("query", ParameterType::STRING,
        "Search query", false));
    add_parameter(Parameter("name", ParameterType::STRING,
        "File or folder name", false));
    add_parameter(Parameter("content", ParameterType::STRING,
        "File content", false));
    add_parameter(Parameter("mime_type", ParameterType::STRING,
        "MIME type (default: text/plain)", false));
    add_parameter(Parameter("max_results", ParameterType::INTEGER,
        "Maximum results (default: 20)", false));
}

std::string GDriveTool::get_string_param(const std::any& value) const {
    if (value.type() == typeid(std::string)) {
        return std::any_cast<std::string>(value);
    } else if (value.type() == typeid(const char*)) {
        return std::string(std::any_cast<const char*>(value));
    }
    throw std::runtime_error("Parameter is not a string");
}

int GDriveTool::get_int_param(const std::any& value) const {
    if (value.type() == typeid(int)) {
        return std::any_cast<int>(value);
    } else if (value.type() == typeid(double)) {
        return static_cast<int>(std::any_cast<double>(value));
    } else if (value.type() == typeid(long)) {
        return static_cast<int>(std::any_cast<long>(value));
    }
    throw std::runtime_error("Parameter is not an integer");
}

std::string GDriveTool::make_api_request(const std::string& endpoint,
                                         const std::string& method,
                                         const std::string& body,
                                         const std::string& content_type) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    std::string url = "https://www.googleapis.com" + endpoint;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token_).c_str());
    headers = curl_slist_append(headers, ("Content-Type: " + content_type).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("API request failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

ToolResult GDriveTool::list_files(const std::string& folder_id, int max_results) {
    std::string query = "'" + folder_id + "' in parents and trashed = false";
    std::string endpoint = "/drive/v3/files?q=" + query +
                          "&pageSize=" + std::to_string(max_results) +
                          "&fields=files(id,name,mimeType,size,modifiedTime,parents)";

    // URL encode the query
    CURL* curl = curl_easy_init();
    char* encoded_query = curl_easy_escape(curl, query.c_str(), query.length());
    endpoint = "/drive/v3/files?q=" + std::string(encoded_query) +
              "&pageSize=" + std::to_string(max_results) +
              "&fields=files(id,name,mimeType,size,modifiedTime,parents)";
    curl_free(encoded_query);
    curl_easy_cleanup(curl);

    std::string response = make_api_request(endpoint);
    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    json files = result.value("files", json::array());
    std::string message = "Found " + std::to_string(files.size()) + " files";

    return ToolResult::Success(result, message);
}

ToolResult GDriveTool::search_files(const std::string& query, int max_results) {
    std::string search_query = "name contains '" + query + "' and trashed = false";

    CURL* curl = curl_easy_init();
    char* encoded_query = curl_easy_escape(curl, search_query.c_str(), search_query.length());
    std::string endpoint = "/drive/v3/files?q=" + std::string(encoded_query) +
                          "&pageSize=" + std::to_string(max_results) +
                          "&fields=files(id,name,mimeType,size,modifiedTime,parents)";
    curl_free(encoded_query);
    curl_easy_cleanup(curl);

    std::string response = make_api_request(endpoint);
    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    json files = result.value("files", json::array());
    std::string message = "Found " + std::to_string(files.size()) + " files matching '" + query + "'";

    return ToolResult::Success(result, message);
}

ToolResult GDriveTool::read_file(const std::string& file_id) {
    // First get file metadata to check type
    std::string meta_endpoint = "/drive/v3/files/" + file_id + "?fields=id,name,mimeType,size";
    std::string meta_response = make_api_request(meta_endpoint);
    json metadata = json::parse(meta_response);

    if (metadata.contains("error")) {
        return ToolResult::Error(metadata["error"]["message"].get<std::string>());
    }

    std::string mime_type = metadata["mimeType"];
    std::string content;

    // Handle Google Docs/Sheets by exporting
    if (mime_type == "application/vnd.google-apps.document") {
        content = export_google_doc(file_id, "text/plain");
    } else if (mime_type == "application/vnd.google-apps.spreadsheet") {
        content = export_google_doc(file_id, "text/csv");
    } else if (mime_type == "application/vnd.google-apps.presentation") {
        content = export_google_doc(file_id, "text/plain");
    } else {
        // Download regular file
        std::string endpoint = "/drive/v3/files/" + file_id + "?alt=media";
        content = make_api_request(endpoint);
    }

    json result = {
        {"file_id", file_id},
        {"name", metadata["name"]},
        {"mime_type", mime_type},
        {"content", content}
    };

    return ToolResult::Success(result, "Read file: " + metadata["name"].get<std::string>());
}

std::string GDriveTool::export_google_doc(const std::string& file_id, const std::string& mime_type) const {
    CURL* curl = curl_easy_init();
    char* encoded_mime = curl_easy_escape(curl, mime_type.c_str(), mime_type.length());
    std::string endpoint = "/drive/v3/files/" + file_id + "/export?mimeType=" + std::string(encoded_mime);
    curl_free(encoded_mime);
    curl_easy_cleanup(curl);

    return make_api_request(endpoint);
}

ToolResult GDriveTool::create_file(const std::string& name, const std::string& content,
                                   const std::string& mime_type, const std::string& folder_id) {
    // Create file metadata
    json metadata = {
        {"name", name},
        {"mimeType", mime_type}
    };

    if (!folder_id.empty() && folder_id != "root") {
        metadata["parents"] = json::array({folder_id});
    }

    // Use multipart upload
    std::string boundary = "----GDriveToolBoundary";
    std::string body = "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    body += metadata.dump() + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Type: " + mime_type + "\r\n\r\n";
    body += content + "\r\n";
    body += "--" + boundary + "--";

    std::string endpoint = "/upload/drive/v3/files?uploadType=multipart&fields=id,name,mimeType,size";
    std::string response = make_api_request(endpoint, "POST", body,
                                            "multipart/related; boundary=" + boundary);

    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "Created file: " + name);
}

ToolResult GDriveTool::create_folder(const std::string& name, const std::string& parent_folder_id) {
    json metadata = {
        {"name", name},
        {"mimeType", "application/vnd.google-apps.folder"}
    };

    if (!parent_folder_id.empty() && parent_folder_id != "root") {
        metadata["parents"] = json::array({parent_folder_id});
    }

    std::string endpoint = "/drive/v3/files?fields=id,name,mimeType";
    std::string response = make_api_request(endpoint, "POST", metadata.dump());

    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "Created folder: " + name);
}

ToolResult GDriveTool::upload_content(const std::string& file_id, const std::string& content) {
    std::string endpoint = "/upload/drive/v3/files/" + file_id + "?uploadType=media";
    std::string response = make_api_request(endpoint, "PATCH", content, "text/plain");

    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "Updated file content");
}

ToolResult GDriveTool::move_file(const std::string& file_id, const std::string& destination_folder_id) {
    // First get current parents
    std::string meta_endpoint = "/drive/v3/files/" + file_id + "?fields=parents";
    std::string meta_response = make_api_request(meta_endpoint);
    json metadata = json::parse(meta_response);

    if (metadata.contains("error")) {
        return ToolResult::Error(metadata["error"]["message"].get<std::string>());
    }

    // Build remove parents string
    std::string remove_parents;
    if (metadata.contains("parents")) {
        for (const auto& parent : metadata["parents"]) {
            if (!remove_parents.empty()) remove_parents += ",";
            remove_parents += parent.get<std::string>();
        }
    }

    // Move file
    std::string endpoint = "/drive/v3/files/" + file_id +
                          "?addParents=" + destination_folder_id +
                          "&removeParents=" + remove_parents +
                          "&fields=id,name,parents";

    std::string response = make_api_request(endpoint, "PATCH", "");
    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "Moved file to folder " + destination_folder_id);
}

ToolResult GDriveTool::delete_file(const std::string& file_id) {
    std::string endpoint = "/drive/v3/files/" + file_id;
    std::string response = make_api_request(endpoint, "DELETE");

    // DELETE returns empty on success
    if (response.empty()) {
        json result = {{"deleted", true}, {"file_id", file_id}};
        return ToolResult::Success(result, "Deleted file " + file_id);
    }

    json result = json::parse(response);
    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "Deleted file " + file_id);
}

ToolResult GDriveTool::get_file_info(const std::string& file_id) {
    std::string endpoint = "/drive/v3/files/" + file_id +
                          "?fields=id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink";
    std::string response = make_api_request(endpoint);

    json result = json::parse(response);

    if (result.contains("error")) {
        return ToolResult::Error(result["error"]["message"].get<std::string>());
    }

    return ToolResult::Success(result, "File info: " + result["name"].get<std::string>());
}

ToolResult GDriveTool::execute_authenticated(const std::map<std::string, std::any>& params) {
    try {
        std::string operation = get_string_param(params.at("operation"));

        if (operation == "list") {
            std::string folder_id = "root";
            int max_results = 20;

            if (params.count("folder_id")) {
                folder_id = get_string_param(params.at("folder_id"));
            }
            if (params.count("max_results")) {
                max_results = get_int_param(params.at("max_results"));
            }

            return list_files(folder_id, max_results);

        } else if (operation == "search") {
            if (!params.count("query")) {
                return ToolResult::Error("search operation requires 'query' parameter");
            }

            std::string query = get_string_param(params.at("query"));
            int max_results = 20;

            if (params.count("max_results")) {
                max_results = get_int_param(params.at("max_results"));
            }

            return search_files(query, max_results);

        } else if (operation == "read") {
            if (!params.count("file_id")) {
                return ToolResult::Error("read operation requires 'file_id' parameter");
            }

            return read_file(get_string_param(params.at("file_id")));

        } else if (operation == "create") {
            if (!params.count("name")) {
                return ToolResult::Error("create operation requires 'name' parameter");
            }

            std::string name = get_string_param(params.at("name"));
            std::string content = params.count("content") ? get_string_param(params.at("content")) : "";
            std::string mime_type = params.count("mime_type") ? get_string_param(params.at("mime_type")) : "text/plain";
            std::string folder_id = params.count("folder_id") ? get_string_param(params.at("folder_id")) : "root";

            return create_file(name, content, mime_type, folder_id);

        } else if (operation == "create_folder") {
            if (!params.count("name")) {
                return ToolResult::Error("create_folder operation requires 'name' parameter");
            }

            std::string name = get_string_param(params.at("name"));
            std::string folder_id = params.count("folder_id") ? get_string_param(params.at("folder_id")) : "root";

            return create_folder(name, folder_id);

        } else if (operation == "upload") {
            if (!params.count("file_id") || !params.count("content")) {
                return ToolResult::Error("upload operation requires 'file_id' and 'content' parameters");
            }

            return upload_content(get_string_param(params.at("file_id")),
                                 get_string_param(params.at("content")));

        } else if (operation == "move") {
            if (!params.count("file_id") || !params.count("destination_folder_id")) {
                return ToolResult::Error("move operation requires 'file_id' and 'destination_folder_id' parameters");
            }

            return move_file(get_string_param(params.at("file_id")),
                            get_string_param(params.at("destination_folder_id")));

        } else if (operation == "delete") {
            if (!params.count("file_id")) {
                return ToolResult::Error("delete operation requires 'file_id' parameter");
            }

            return delete_file(get_string_param(params.at("file_id")));

        } else if (operation == "get_info") {
            if (!params.count("file_id")) {
                return ToolResult::Error("get_info operation requires 'file_id' parameter");
            }

            return get_file_info(get_string_param(params.at("file_id")));

        } else {
            return ToolResult::Error("Unknown operation: " + operation);
        }

    } catch (const std::exception& e) {
        return ToolResult::Error(std::string("GDrive error: ") + e.what());
    }
}
