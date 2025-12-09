#ifndef API_SERVER_STATELESS_HPP
#define API_SERVER_STATELESS_HPP

#include <httplib.h>
#include <memory>
#include "SupabaseClient.hpp"
#include "ToolRegistry.hpp"
#include "CompletionEngine.hpp"
#include "AuthManager.hpp"

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
    bool authenticate_request(const httplib::Request& req,
                             httplib::Response& res,
                             std::string& client_id_out);

public:
    ApiServer(int port, std::shared_ptr<SupabaseClient> db, std::shared_ptr<AuthManager> auth);

    // Start the server (blocking)
    void start();

    // Stop the server
    void stop();
};

#endif // API_SERVER_STATELESS_HPP
