#ifndef API_SERVER_STATELESS_HPP
#define API_SERVER_STATELESS_HPP

#include <httplib.h>
#include <memory>
#include "SupabaseClient.hpp"
#include "ToolRegistry.hpp"
#include "CompletionEngine.hpp"

class ApiServer {
private:
    httplib::Server server_;
    std::shared_ptr<SupabaseClient> db_;
    std::string default_client_id_;
    int port_;

    void configure_cors();
    void configure_routes();

    // Helper methods for stateless operation
    std::shared_ptr<ToolRegistry> build_tool_registry(const std::vector<std::string>& tool_names);
    std::shared_ptr<CompletionEngine> create_engine(const std::string& engine_type);

public:
    ApiServer(int port, std::shared_ptr<SupabaseClient> db, const std::string& client_id = "00000000-0000-0000-0000-000000000000");

    // Start the server (blocking)
    void start();

    // Stop the server
    void stop();
};

#endif // API_SERVER_STATELESS_HPP
