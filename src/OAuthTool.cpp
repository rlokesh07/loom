#include "../include/OAuthTool.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>

using json = nlohmann::json;

// Callback for CURL to write response
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

OAuthTool::OAuthTool(const std::string& name,
                     const std::string& description,
                     const std::string& provider)
    : Tool(name, description), provider_(provider) {
}

void OAuthTool::set_session_context(const std::string& session_id) {
    session_id_ = session_id;
}

bool OAuthTool::is_token_valid() const {
    if (access_token_.empty()) return false;
    return std::chrono::system_clock::now() < token_expiry_;
}

bool OAuthTool::authenticate() {
    // If we have a valid token in memory, use it
    if (is_token_valid()) {
        return true;
    }

    // Get tokens from store
    auto tokens_opt = SessionOAuthStore::get_tokens(session_id_, provider_);

    if (!tokens_opt) {
        return false; // No tokens available
    }

    auto tokens = *tokens_opt;

    // Check if token is expired
    if (std::chrono::system_clock::now() >= tokens.expiry) {
        // Try to refresh
        if (!refresh_access_token()) {
            return false;
        }

        // Get updated tokens after refresh
        tokens_opt = SessionOAuthStore::get_tokens(session_id_, provider_);
        if (!tokens_opt) {
            return false;
        }
        tokens = *tokens_opt;
    }

    // Set token for use
    access_token_ = tokens.access_token;
    token_expiry_ = tokens.expiry;

    return true;
}

bool OAuthTool::refresh_access_token() {
    auto tokens_opt = SessionOAuthStore::get_tokens(session_id_, provider_);

    if (!tokens_opt) {
        return false;
    }

    auto tokens = *tokens_opt;

    if (tokens.refresh_token.empty()) {
        return false;
    }

    // Get OAuth config for this tool
    auto config = OAuthRegistry::get_config(get_name());

    // Prepare token refresh request based on provider
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string response;
    std::string post_data;

    if (provider_ == "google") {
        // Google-specific token refresh
        const char* client_id = std::getenv("GOOGLE_OAUTH_CLIENT_ID");
        const char* client_secret = std::getenv("GOOGLE_OAUTH_CLIENT_SECRET");

        if (!client_id || !client_secret) {
            curl_easy_cleanup(curl);
            return false;
        }

        post_data = "client_id=" + std::string(client_id) +
                   "&client_secret=" + std::string(client_secret) +
                   "&refresh_token=" + tokens.refresh_token +
                   "&grant_type=refresh_token";

    } else if (provider_ == "microsoft") {
        // Microsoft-specific token refresh
        const char* client_id = std::getenv("MICROSOFT_OAUTH_CLIENT_ID");
        const char* client_secret = std::getenv("MICROSOFT_OAUTH_CLIENT_SECRET");

        if (!client_id || !client_secret) {
            curl_easy_cleanup(curl);
            return false;
        }

        post_data = "client_id=" + std::string(client_id) +
                   "&client_secret=" + std::string(client_secret) +
                   "&refresh_token=" + tokens.refresh_token +
                   "&grant_type=refresh_token";
    } else {
        // Unsupported provider
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, config.token_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return false;
    }

    try {
        json token_response = json::parse(response);

        if (token_response.contains("access_token")) {
            std::string new_access_token = token_response["access_token"];
            int expires_in = token_response.value("expires_in", 3600);

            auto new_expiry = std::chrono::system_clock::now() +
                             std::chrono::seconds(expires_in - 60); // 60s buffer

            // Update in store
            SessionOAuthStore::update_access_token(
                session_id_,
                provider_,
                new_access_token,
                new_expiry
            );

            return true;
        }
    } catch (...) {
        return false;
    }

    return false;
}

ToolResult OAuthTool::execute(const std::map<std::string, std::any>& params) {
    // Validate session context is set
    if (session_id_.empty()) {
        return ToolResult::Error(
            "Tool not properly initialized with session context"
        );
    }

    // Authenticate
    if (!authenticate()) {
        return ToolResult::Error(
            provider_ + " authentication required. Please connect via OAuth at " +
            "/api/sessions/" + session_id_ + "/oauth/" + provider_
        );
    }

    // Call subclass implementation
    return execute_authenticated(params);
}

// Static OAuth flow helpers

std::string OAuthTool::url_encode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;

    char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}

std::vector<std::string> OAuthTool::parse_scopes(const std::string& scope_string) {
    std::vector<std::string> scopes;
    std::string current;

    for (char c : scope_string) {
        if (c == ' ' || c == ',') {
            if (!current.empty()) {
                scopes.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        scopes.push_back(current);
    }

    return scopes;
}

std::string OAuthTool::build_authorization_url(
    const std::string& provider,
    const std::vector<std::string>& scopes,
    const std::string& redirect_uri,
    const std::string& state
) {
    std::string client_id;
    std::string auth_base_url;

    if (provider == "google") {
        const char* id = std::getenv("GOOGLE_OAUTH_CLIENT_ID");
        if (!id) {
            throw std::runtime_error("GOOGLE_OAUTH_CLIENT_ID environment variable not set");
        }
        client_id = id;
        auth_base_url = "https://accounts.google.com/o/oauth2/v2/auth";

    } else if (provider == "microsoft") {
        const char* id = std::getenv("MICROSOFT_OAUTH_CLIENT_ID");
        if (!id) {
            throw std::runtime_error("MICROSOFT_OAUTH_CLIENT_ID environment variable not set");
        }
        client_id = id;
        auth_base_url = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";

    } else {
        throw std::runtime_error("Unsupported OAuth provider: " + provider);
    }

    // Build scope string
    std::string scope_str;
    for (size_t i = 0; i < scopes.size(); ++i) {
        if (i > 0) scope_str += " ";
        scope_str += scopes[i];
    }

    // Build URL
    std::string url = auth_base_url +
                     "?client_id=" + url_encode(client_id) +
                     "&redirect_uri=" + url_encode(redirect_uri) +
                     "&response_type=code" +
                     "&scope=" + url_encode(scope_str) +
                     "&access_type=offline" +
                     "&prompt=consent";

    if (!state.empty()) {
        url += "&state=" + url_encode(state);
    }

    return url;
}

OAuthTokens OAuthTool::exchange_authorization_code(
    const std::string& code,
    const std::string& provider,
    const std::string& redirect_uri
) {
    std::string client_id;
    std::string client_secret;
    std::string token_url;

    if (provider == "google") {
        const char* id = std::getenv("GOOGLE_OAUTH_CLIENT_ID");
        const char* secret = std::getenv("GOOGLE_OAUTH_CLIENT_SECRET");

        if (!id || !secret) {
            throw std::runtime_error("GOOGLE_OAUTH_CLIENT_ID or GOOGLE_OAUTH_CLIENT_SECRET not set");
        }

        client_id = id;
        client_secret = secret;
        token_url = "https://oauth2.googleapis.com/token";

    } else if (provider == "microsoft") {
        const char* id = std::getenv("MICROSOFT_OAUTH_CLIENT_ID");
        const char* secret = std::getenv("MICROSOFT_OAUTH_CLIENT_SECRET");

        if (!id || !secret) {
            throw std::runtime_error("MICROSOFT_OAUTH_CLIENT_ID or MICROSOFT_OAUTH_CLIENT_SECRET not set");
        }

        client_id = id;
        client_secret = secret;
        token_url = "https://login.microsoftonline.com/common/oauth2/v2.0/token";

    } else {
        throw std::runtime_error("Unsupported OAuth provider: " + provider);
    }

    // Prepare POST data
    std::string post_data = "code=" + url_encode(code) +
                           "&client_id=" + url_encode(client_id) +
                           "&client_secret=" + url_encode(client_secret) +
                           "&redirect_uri=" + url_encode(redirect_uri) +
                           "&grant_type=authorization_code";

    // Make HTTP request
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, token_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("Token exchange failed: " + std::string(curl_easy_strerror(res)));
    }

    // Parse response
    try {
        json token_response = json::parse(response);

        if (token_response.contains("error")) {
            throw std::runtime_error("OAuth error: " + token_response["error"].get<std::string>());
        }

        if (!token_response.contains("access_token")) {
            throw std::runtime_error("No access_token in response");
        }

        OAuthTokens tokens;
        tokens.access_token = token_response["access_token"];
        tokens.refresh_token = token_response.value("refresh_token", "");

        int expires_in = token_response.value("expires_in", 3600);
        tokens.expiry = std::chrono::system_clock::now() +
                       std::chrono::seconds(expires_in - 60); // 60s buffer

        // Parse scopes
        if (token_response.contains("scope")) {
            std::string scope_str = token_response["scope"];
            tokens.granted_scopes = parse_scopes(scope_str);
        }

        return tokens;

    } catch (const json::exception& e) {
        throw std::runtime_error("Failed to parse token response: " + std::string(e.what()));
    }
}
