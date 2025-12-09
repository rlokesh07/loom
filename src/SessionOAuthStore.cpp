#include "../include/SessionOAuthStore.hpp"
#include <algorithm>

std::unordered_map<std::string, std::unordered_map<std::string, OAuthTokens>> SessionOAuthStore::store_;

void SessionOAuthStore::store_tokens(
    const std::string& session_id,
    const std::string& provider,
    const OAuthTokens& tokens
) {
    store_[session_id][provider] = tokens;
}

std::optional<OAuthTokens> SessionOAuthStore::get_tokens(
    const std::string& session_id,
    const std::string& provider
) {
    if (store_.count(session_id) && store_[session_id].count(provider)) {
        return store_[session_id][provider];
    }

    return std::nullopt;
}

bool SessionOAuthStore::has_valid_tokens(
    const std::string& session_id,
    const std::string& provider
) {
    if (!store_.count(session_id) || !store_[session_id].count(provider)) {
        return false;
    }

    const auto& tokens = store_[session_id][provider];
    auto now = std::chrono::system_clock::now();

    return now < tokens.expiry;
}

bool SessionOAuthStore::has_tokens(
    const std::string& session_id,
    const std::string& provider
) {
    return store_.count(session_id) && store_[session_id].count(provider);
}

void SessionOAuthStore::update_access_token(
    const std::string& session_id,
    const std::string& provider,
    const std::string& new_access_token,
    std::chrono::time_point<std::chrono::system_clock> new_expiry
) {
    if (store_.count(session_id) && store_[session_id].count(provider)) {
        store_[session_id][provider].access_token = new_access_token;
        store_[session_id][provider].expiry = new_expiry;
    }
}

bool SessionOAuthStore::has_sufficient_scopes(
    const std::string& session_id,
    const std::string& provider,
    const std::vector<std::string>& required_scopes
) {
    if (!store_.count(session_id) || !store_[session_id].count(provider)) {
        return false;
    }

    const auto& granted_scopes = store_[session_id][provider].granted_scopes;

    // Check if all required scopes are in granted scopes
    for (const auto& required : required_scopes) {
        if (std::find(granted_scopes.begin(), granted_scopes.end(), required) == granted_scopes.end()) {
            return false;
        }
    }

    return true;
}

void SessionOAuthStore::remove_tokens(
    const std::string& session_id,
    const std::string& provider
) {
    if (store_.count(session_id)) {
        store_[session_id].erase(provider);

        // If session has no more providers, remove session entry
        if (store_[session_id].empty()) {
            store_.erase(session_id);
        }
    }
}

void SessionOAuthStore::remove_session(const std::string& session_id) {
    store_.erase(session_id);
}

std::vector<std::string> SessionOAuthStore::get_authenticated_providers(
    const std::string& session_id
) {
    std::vector<std::string> providers;

    if (store_.count(session_id)) {
        for (const auto& [provider, tokens] : store_[session_id]) {
            providers.push_back(provider);
        }
    }

    return providers;
}

void SessionOAuthStore::clear_all() {
    store_.clear();
}
