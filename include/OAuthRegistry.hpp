#ifndef OAUTH_REGISTRY_HPP
#define OAUTH_REGISTRY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * @brief Configuration for an OAuth-enabled tool
 */
struct OAuthToolConfig {
    std::string provider;           // "google", "microsoft", "slack", etc.
    std::vector<std::string> scopes; // OAuth scopes required
    std::string auth_url_base;      // Base URL for OAuth authorization
    std::string token_url;          // URL for token exchange/refresh
};

/**
 * @brief Registry of OAuth tool configurations
 *
 * Maintains mappings between tool names and their OAuth requirements.
 * Supports multiple tools sharing the same provider (e.g., gdrive and gmail both use Google).
 */
class OAuthRegistry {
private:
    static std::unordered_map<std::string, OAuthToolConfig> configs_;
    static bool initialized_;

public:
    /**
     * @brief Initialize the registry with tool configurations
     */
    static void initialize();

    /**
     * @brief Check if a tool requires OAuth authentication
     */
    static bool requires_oauth(const std::string& tool_name);

    /**
     * @brief Get OAuth configuration for a tool
     * @throws std::out_of_range if tool not found
     */
    static const OAuthToolConfig& get_config(const std::string& tool_name);

    /**
     * @brief Get provider name for a tool
     * @throws std::out_of_range if tool not found
     */
    static std::string get_provider(const std::string& tool_name);

    /**
     * @brief Get required scopes for a tool
     * @throws std::out_of_range if tool not found
     */
    static std::vector<std::string> get_scopes(const std::string& tool_name);

    /**
     * @brief Check if registry is initialized
     */
    static bool is_initialized() { return initialized_; }

    /**
     * @brief Get all registered OAuth tools
     */
    static std::vector<std::string> list_oauth_tools();
};

#endif // OAUTH_REGISTRY_HPP
