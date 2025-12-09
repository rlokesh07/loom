#ifndef SUPABASE_CLIENT_HPP
#define SUPABASE_CLIENT_HPP

#include <curl/curl.h>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include "Agent.hpp"

// Stateless session metadata (no Agent pointer!)
struct SessionMetadata {
    std::string id;
    std::string client_id;
    std::string engine_type;  // "chatgpt" or "ollama"
    std::string model;
    std::string system_prompt;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    std::chrono::time_point<std::chrono::system_clock> last_accessed_at;
};




class SupabaseClient{
    private:
        std::string apiKey;
        std::string baseURL;

        // Helper function to make HTTP requests to Supabase REST API
        std::string makeRequest(const std::string& method, const std::string& endpoint,
                               const nlohmann::json& body = nlohmann::json::object(),
                               const std::string& query = "");

    public:
        SupabaseClient(std::string api_key, std::string base_url): apiKey(api_key), baseURL(base_url) {}

        // returns the session ID, or empty optional if creation fails
        std::optional<std::string> createSession(const std::string& engine_type, const std::string& model,
                                                 const std::string& clientID, const std::string& system_prompt = "");

        std::optional<SessionMetadata> getSession(const std::string& sessionID, const std::string& clientID);
        
        // returns whether or not the operation was successfull 
        bool deleteSession(const std::string& sessionID, const std::string& clientID);
       
        // add message to a conversation
        void add_message(const std::string& sessionid, const std::string& clientid,
                const std::string& role, const std::string& content);

        std::vector<ChatMessage> getConversationHistory(const std::string& sessionid,
                const std::string& clientid);

        // returns whether or not the operation was successfull
        bool clearHistory(const std::string& sessionID, const std::string& clientID);

        // returns whether or not the operation was successfull
        bool registerTools(const std::string& sessionID, const std::string& clientID,
                const std::vector<std::string>& tool_names);

        std::vector<std::string> getSessionTools(const std::string& sessionID, const std::string& clientID);

        // returns whether or not the operation was successfull
        bool unregisterTools(const std::string& sessionID, const std::string& clientID,
                const std::vector<std::string>& tool_names);

        // returns whether or not the operation was successfull
        bool clearTools(const std::string& sessionID, const std::string& clientID);

        // Child session operations (for agent-as-tool support)
        // returns whether or not the operation was successfull
        bool registerChildSession(const std::string& parentSessionID, const std::string& toolName,
                const std::string& childSessionID, const std::string& clientID);

        std::optional<std::string> getChildSession(const std::string& parentSessionID,
                const std::string& toolName, const std::string& clientID);

        // returns whether or not the operation was successfull
        bool unregisterChildSession(const std::string& parentSessionID, const std::string& toolName,
                const std::string& clientID);

        // List all sessions for a client
        std::vector<SessionMetadata> listClientSessions(const std::string& client_id);

        // ===== Multi-Tenancy Authentication Methods =====

        // Authenticate API key and return client_id
        // Returns client_id if api_key_hash matches, nullopt otherwise
        std::optional<std::string> authenticate_client(const std::string& api_key_hash);

        // Create new client with hashed API key
        // Returns client_id on success, nullopt on failure
        std::optional<std::string> create_client(const std::string& api_key_hash,
                                                  const std::string& display_name);

        // Set RLS session variable for this connection
        // Must be called before any queries to enforce Row Level Security
        void set_client_context(const std::string& client_id);
};

#endif // SUPABASE_CLIENT_HPP
