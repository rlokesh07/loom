#ifndef AUTH_MANAGER_HPP
#define AUTH_MANAGER_HPP

#include <string>
#include <optional>
#include <memory>
#include <utility>
#include "SupabaseClient.hpp"

class AuthManager {
private:
    std::shared_ptr<SupabaseClient> db_;

    // Generate random API key with format: sk-{32 random chars}
    std::string generate_api_key();

    // SHA-256 hash for storing API keys
    std::string hash_api_key(const std::string& api_key);

public:
    // Constructor
    AuthManager(std::shared_ptr<SupabaseClient> db) : db_(db) {}

    // Create new client, return (client_id, api_key)
    // The plain API key is ONLY returned here - never stored or shown again
    std::optional<std::pair<std::string, std::string>> create_client(
        const std::string& display_name
    );

    // Validate API key, return client_id if valid
    std::optional<std::string> authenticate(const std::string& api_key);

    // Extract "Bearer sk-xxx" from Authorization header
    // Returns the API key without the "Bearer " prefix
    static std::optional<std::string> extract_api_key(const std::string& auth_header);
};

#endif // AUTH_MANAGER_HPP
