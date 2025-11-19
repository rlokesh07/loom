#include "ApiServer.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

static ApiServer* server_ptr = nullptr;

void signal_handler(int signal) {
    if (server_ptr) {
        std::cout << "\nShutting down server..." << std::endl;
        server_ptr->stop();
    }
}

int main() {
    // Get port from environment or use default
    int port = 8080;
    const char* port_env = std::getenv("LOOM_API_PORT");
    if (port_env) {
        port = std::atoi(port_env);
    }

    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║                    Loom API Server                         ║
║                                                            ║
║  REST API for the Loom Agent System                        ║
║                                                            ║
║  Environment variables:                                    ║
║  - LOOM_API_PORT: Server port (default: 8080)              ║
║  - OPENAI_API_KEY: Required for ChatGPT agents             ║
║  - BRAVE_SEARCH_API_KEY: Required for WebSearchTool        ║
║  - GITHUB_TOKEN: Required for GitHubTool                   ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    ApiServer server(port);
    server_ptr = &server;

    // Handle Ctrl+C gracefully
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.start();

    return 0;
}
