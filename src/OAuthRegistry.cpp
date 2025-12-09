#include "../include/OAuthRegistry.hpp"
#include <stdexcept>

std::unordered_map<std::string, OAuthToolConfig> OAuthRegistry::configs_;
bool OAuthRegistry::initialized_ = false;

void OAuthRegistry::initialize() {
    if (initialized_) return;

    // Google Drive
    configs_["gdrive"] = {
        .provider = "google",
        .scopes = {"https://www.googleapis.com/auth/drive"},
        .auth_url_base = "https://accounts.google.com/o/oauth2/v2/auth",
        .token_url = "https://oauth2.googleapis.com/token"
    };

    // Gmail (future)
    configs_["gmail"] = {
        .provider = "google",
        .scopes = {
            "https://www.googleapis.com/auth/gmail.readonly",
            "https://www.googleapis.com/auth/gmail.send"
        },
        .auth_url_base = "https://accounts.google.com/o/oauth2/v2/auth",
        .token_url = "https://oauth2.googleapis.com/token"
    };

    // Google Calendar (future)
    configs_["gcalendar"] = {
        .provider = "google",
        .scopes = {"https://www.googleapis.com/auth/calendar"},
        .auth_url_base = "https://accounts.google.com/o/oauth2/v2/auth",
        .token_url = "https://oauth2.googleapis.com/token"
    };

    // Microsoft OneDrive (future)
    configs_["onedrive"] = {
        .provider = "microsoft",
        .scopes = {"Files.ReadWrite.All", "offline_access"},
        .auth_url_base = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize",
        .token_url = "https://login.microsoftonline.com/common/oauth2/v2.0/token"
    };

    // Notion (future)
    configs_["notion"] = {
        .provider = "notion",
        .scopes = {},
        .auth_url_base = "https://api.notion.com/v1/oauth/authorize",
        .token_url = "https://api.notion.com/v1/oauth/token"
    };

    // Slack (future)
    configs_["slack"] = {
        .provider = "slack",
        .scopes = {"channels:read", "chat:write", "users:read"},
        .auth_url_base = "https://slack.com/oauth/v2/authorize",
        .token_url = "https://slack.com/api/oauth.v2.access"
    };

    initialized_ = true;
}

bool OAuthRegistry::requires_oauth(const std::string& tool_name) {
    if (!initialized_) initialize();
    return configs_.count(tool_name) > 0;
}

const OAuthToolConfig& OAuthRegistry::get_config(const std::string& tool_name) {
    if (!initialized_) initialize();
    return configs_.at(tool_name);
}

std::string OAuthRegistry::get_provider(const std::string& tool_name) {
    if (!initialized_) initialize();
    return configs_.at(tool_name).provider;
}

std::vector<std::string> OAuthRegistry::get_scopes(const std::string& tool_name) {
    if (!initialized_) initialize();
    return configs_.at(tool_name).scopes;
}

std::vector<std::string> OAuthRegistry::list_oauth_tools() {
    if (!initialized_) initialize();

    std::vector<std::string> tools;
    tools.reserve(configs_.size());

    for (const auto& [tool_name, config] : configs_) {
        tools.push_back(tool_name);
    }

    return tools;
}
