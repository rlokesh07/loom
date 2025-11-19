#ifndef API_SERVER_HPP
#define API_SERVER_HPP

#include <httplib.h>
#include "AgentSessionManager.hpp"

class ApiServer {
private:
    httplib::Server server_;
    AgentSessionManager session_manager_;
    int port_;

    void configure_cors();
    void configure_routes();

public:
    ApiServer(int port = 8080);

    // Start the server (blocking)
    void start();

    // Stop the server
    void stop();

    // Get the session manager (for testing)
    AgentSessionManager& get_session_manager();
};

#endif // API_SERVER_HPP
