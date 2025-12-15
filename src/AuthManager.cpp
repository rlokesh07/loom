#include "AuthManager.hpp"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>

std::string AuthManager::generate_api_key() {
    // Format: sk-{32 random alphanumeric characters}
    const std::string prefix = "sk-";
    const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const int key_length = 32;

    std::string random_part;
    random_part.reserve(key_length);

    // Use OpenSSL's random number generator for cryptographic security
    unsigned char random_bytes[key_length];
    if (RAND_bytes(random_bytes, key_length) != 1) {
        // Fallback to C++ random if OpenSSL fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, charset.size() - 1);

        for (int i = 0; i < key_length; ++i) {
            random_part += charset[dis(gen)];
        }
    } else {
        // Convert random bytes to charset characters
        for (int i = 0; i < key_length; ++i) {
            random_part += charset[random_bytes[i] % charset.size()];
        }
    }

    return prefix + random_part;
}

std::string AuthManager::hash_api_key(const std::string& api_key) {
    // Compute SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(api_key.c_str()),
           api_key.length(),
           hash);

    // Convert to hex string
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

std::optional<std::pair<std::string, std::string>> AuthManager::create_client(
    const std::string& display_name
) {
    // Create client in database (no API key in clients table anymore)
    auto client_id_opt = db_->create_client(display_name);

    if (!client_id_opt) {
        return std::nullopt;
    }

    std::string client_id = *client_id_opt;

    // Generate base API key using new api_key_aliases system
    auto base_key_opt = db_->createBaseApiKey(client_id);

    if (!base_key_opt) {
        // Client created but key generation failed
        return std::nullopt;
    }

    // Return both client_id and the plain API key
    // This is the ONLY time the plain API key is returned
    return std::make_pair(client_id, *base_key_opt);
}

std::optional<std::string> AuthManager::authenticate(const std::string& api_key) {
    // Hash the provided API key
    std::string api_key_hash = hash_api_key(api_key);

    // Look up in api_key_aliases table
    auto alias_opt = db_->authenticateApiKey(api_key_hash);
    if (!alias_opt) {
        return std::nullopt;
    }

    return alias_opt->client_id;
}

std::optional<std::string> AuthManager::extract_api_key(const std::string& auth_header) {
    // Expected format: "Bearer sk-xxxxxxxxxxxxx"
    const std::string bearer_prefix = "Bearer ";

    // Check if header starts with "Bearer "
    if (auth_header.size() <= bearer_prefix.size() ||
        auth_header.substr(0, bearer_prefix.size()) != bearer_prefix) {
        return std::nullopt;
    }

    // Extract the API key (everything after "Bearer ")
    std::string api_key = auth_header.substr(bearer_prefix.size());

    // Trim whitespace
    api_key.erase(0, api_key.find_first_not_of(" \t\n\r"));
    api_key.erase(api_key.find_last_not_of(" \t\n\r") + 1);

    // Validate format (should start with "sk-")
    if (api_key.size() < 3 || api_key.substr(0, 3) != "sk-") {
        return std::nullopt;
    }

    return api_key;
}
