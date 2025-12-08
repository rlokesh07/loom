#ifndef SESSION_OAUTH_STORE_HPP
#define SESSION_OAUTH_STORE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>

/**
 * @brief OAuth tokens for a provider
 */
struct OAuthTokens {
    std::string access_token;
    std::string refresh_token;
    std::chrono::time_point<std::chrono::system_clock> expiry;
    std::vector<std::string> granted_scopes;
};

/**
 * @brief Storage for OAuth tokens per session and provider
 *
 * Stores tokens in memory indexed by session_id and provider.
 * Tokens are automatically removed when sessions are destroyed.
 *
 * Storage structure:
 *   session_id -> provider -> OAuthTokens
 *
 * Example:
 *   "sess-123" -> "google" -> { access_token, refresh_token, ... }
 *   "sess-123" -> "microsoft" -> { access_token, refresh_token, ... }
 *   "sess-456" -> "google" -> { access_token, refresh_token, ... }
 */
class SessionOAuthStore {
private:
    // session_id -> (provider -> tokens)
    static std::unordered_map<
        std::string,
        std::unordered_map<std::string, OAuthTokens>
    > store_;

public:
    /**
     * @brief Store tokens for a session and provider
     */
    static void store_tokens(
        const std::string& session_id,
        const std::string& provider,
        const OAuthTokens& tokens
    );

    /**
     * @brief Get tokens for a session and provider
     * @return Tokens if found, nullopt otherwise
     */
    static std::optional<OAuthTokens> get_tokens(
        const std::string& session_id,
        const std::string& provider
    );

    /**
     * @brief Check if session has valid (non-expired) tokens for a provider
     */
    static bool has_valid_tokens(
        const std::string& session_id,
        const std::string& provider
    );

    /**
     * @brief Check if session has any tokens for a provider (expired or not)
     */
    static bool has_tokens(
        const std::string& session_id,
        const std::string& provider
    );

    /**
     * @brief Update access token and expiry (after refresh)
     */
    static void update_access_token(
        const std::string& session_id,
        const std::string& provider,
        const std::string& new_access_token,
        std::chrono::time_point<std::chrono::system_clock> new_expiry
    );

    /**
     * @brief Check if tokens have sufficient scopes
     */
    static bool has_sufficient_scopes(
        const std::string& session_id,
        const std::string& provider,
        const std::vector<std::string>& required_scopes
    );

    /**
     * @brief Remove tokens for a specific provider in a session
     */
    static void remove_tokens(
        const std::string& session_id,
        const std::string& provider
    );

    /**
     * @brief Remove all tokens for a session (when session is destroyed)
     */
    static void remove_session(const std::string& session_id);

    /**
     * @brief Get all providers authenticated for a session
     */
    static std::vector<std::string> get_authenticated_providers(
        const std::string& session_id
    );

    /**
     * @brief Clear all stored tokens (for testing/cleanup)
     */
    static void clear_all();
};

#endif // SESSION_OAUTH_STORE_HPP
