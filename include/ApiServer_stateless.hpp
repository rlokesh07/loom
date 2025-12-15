#ifndef API_SERVER_STATELESS_HPP
#define API_SERVER_STATELESS_HPP

#include <httplib.h>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "SupabaseClient.hpp"
#include "ToolRegistry.hpp"
#include "CompletionEngine.hpp"
#include "AuthManager.hpp"
#include "Agent.hpp"

using json = nlohmann::json;

class ApiServer {
private:
    httplib::Server server_;
    std::shared_ptr<SupabaseClient> db_;
    std::shared_ptr<AuthManager> auth_;
    int port_;

    void configure_cors();
    void configure_routes();

    // Helper methods for stateless operation
    std::shared_ptr<ToolRegistry> build_tool_registry(const std::vector<std::string>& tool_names);
    std::shared_ptr<CompletionEngine> create_engine(const std::string& engine_type);

    // Authentication helper - handles auth + error responses
    // Returns true if authenticated, false if auth failed (with error set in response)
    // If true, client_id_out contains the authenticated client's ID
    // session_id_out is set if using a session-specific API key
    bool authenticate_request(const httplib::Request& req,
                             httplib::Response& res,
                             std::string& client_id_out,
                             std::optional<std::string>& session_id_out);

    // OpenAI-compatible endpoint handlers
    void handle_chat_completion(const httplib::Request& req, httplib::Response& res);
    void handle_session_completion(httplib::Response& res, const std::string& client_id,
                                   const std::string& session_id, const json& request_body);
    void handle_stateless_completion(httplib::Response& res, const std::string& client_id,
                                     const json& request_body);
    void handle_list_models(const httplib::Request& req, httplib::Response& res);

    // Model mapping and helpers
    static const std::unordered_map<std::string, std::string> MODEL_ENGINE_MAP;
    static std::string infer_engine_from_model(const std::string& model);
    static json build_openai_response(const ToolResult& result, const std::string& model);
    static std::string generate_random_id();

public:
    ApiServer(int port, std::shared_ptr<SupabaseClient> db, std::shared_ptr<AuthManager> auth);

    // Start the server (blocking)
    void start();

    // Stop the server
    void stop();
};

#endif // API_SERVER_STATELESS_HPP
