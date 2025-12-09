#ifndef OAUTH_TOOL_HPP
#define OAUTH_TOOL_HPP

#include "Tool.hpp"
#include "ToolResult.hpp"
#include "SessionOAuthStore.hpp"
#include "OAuthRegistry.hpp"
#include <string>
#include <chrono>

/**
 * @brief Base class for tools that require OAuth authentication
 *
 * Provides automatic authentication handling, token refresh, and session context.
 * Subclasses only need to implement execute_authenticated() instead of execute().
 *
 * Example:
 *   class MyOAuthTool : public OAuthTool {
 *   public:
 *       MyOAuthTool() : OAuthTool("my_tool", "Description", "google") {}
 *   protected:
 *       ToolResult execute_authenticated(const std::map<std::string, std::any>& params) override {
 *           // access_token_ is already available
 *           // Make API calls using access_token_
 *       }
 *   };
 */
class OAuthTool : public Tool {
protected:
    std::string session_id_;
    std::string provider_;
    std::string access_token_;
    std::chrono::time_point<std::chrono::system_clock> token_expiry_;

    /**
     * @brief Authenticate using session's stored tokens
     * @return true if authenticated successfully, false otherwise
     *
     * This method:
     * 1. Retrieves tokens from SessionOAuthStore for this session and provider
     * 2. Checks if token is expired
     * 3. If expired, attempts to refresh the token
     * 4. Sets access_token_ for use by subclass
     */
    virtual bool authenticate();

    /**
     * @brief Refresh the access token using refresh token
     * @return true if refresh successful, false otherwise
     */
    virtual bool refresh_access_token();

    /**
     * @brief Check if current access token is valid (not expired)
     */
    bool is_token_valid() const;

    /**
     * @brief Subclasses implement this instead of execute()
     *
     * When this is called, authentication has already been performed
     * and access_token_ is available for use.
     */
    virtual ToolResult execute_authenticated(
        const std::map<std::string, std::any>& params
    ) = 0;

public:
    /**
     * @brief Constructor
     * @param name Tool name (e.g., "gdrive", "gmail")
     * @param description Tool description
     * @param provider OAuth provider (e.g., "google", "microsoft")
     */
    OAuthTool(const std::string& name,
              const std::string& description,
              const std::string& provider);

    /**
     * @brief Set the session context for this tool instance
     *
     * Must be called before execute() to associate tool with a session's OAuth tokens
     */
    void set_session_context(const std::string& session_id);

    /**
     * @brief Get the OAuth provider for this tool
     */
    std::string get_provider() const { return provider_; }

    /**
     * @brief Execute the tool (final implementation)
     *
     * This method:
     * 1. Validates session context is set
     * 2. Performs authentication
     * 3. Calls execute_authenticated() if auth succeeds
     * 4. Returns appropriate error if auth fails
     */
    ToolResult execute(const std::map<std::string, std::any>& params) final;

    // Static OAuth flow helpers

    /**
     * @brief Exchange authorization code for tokens
     * @param code Authorization code from OAuth callback
     * @param provider OAuth provider (e.g., "google", "microsoft")
     * @param redirect_uri The redirect URI used in the OAuth flow
     * @return OAuthTokens containing access_token, refresh_token, expiry, and scopes
     * @throws std::runtime_error if exchange fails
     */
    static OAuthTokens exchange_authorization_code(
        const std::string& code,
        const std::string& provider,
        const std::string& redirect_uri
    );

    /**
     * @brief Build OAuth authorization URL for frontend
     * @param provider OAuth provider
     * @param scopes Required OAuth scopes
     * @param redirect_uri Where OAuth should redirect after authorization
     * @param state Optional state parameter for CSRF protection
     * @return Full authorization URL
     */
    static std::string build_authorization_url(
        const std::string& provider,
        const std::vector<std::string>& scopes,
        const std::string& redirect_uri,
        const std::string& state = ""
    );

private:
    /**
     * @brief URL encode a string
     */
    static std::string url_encode(const std::string& str);

    /**
     * @brief Parse scope string into vector
     */
    static std::vector<std::string> parse_scopes(const std::string& scope_string);
};

#endif // OAUTH_TOOL_HPP
