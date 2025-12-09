#include "../include/ApiServer_stateless.hpp"
#include "../include/SupabaseClient.hpp"
#include "../include/AuthManager.hpp"
#include <iostream>
#include <cstdlib>
#include <memory>

int main(int argc, char* argv[]) {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║           Loom API Server (Stateless Version)              ║
║                                                            ║
║  Stateless architecture with Supabase backend             ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Get Supabase configuration from environment
    const char* supabase_url = std::getenv("SUPABASE_URL");
    const char* supabase_key = std::getenv("SUPABASE_KEY");

    if (!supabase_url || !supabase_key) {
        std::cerr << "❌ ERROR: SUPABASE_URL and SUPABASE_KEY environment variables must be set!" << std::endl;
        std::cerr << "\nPlease set your Supabase configuration:" << std::endl;
        std::cerr << "  export SUPABASE_URL='https://your-project.supabase.co'" << std::endl;
        std::cerr << "  export SUPABASE_KEY='your-anon-key'" << std::endl;
        std::cerr << "\nYou can get these from your Supabase project settings." << std::endl;
        return 1;
    }

    std::cout << "✓ SUPABASE_URL is set" << std::endl;
    std::cout << "✓ SUPABASE_KEY is set" << std::endl;

    // Get port from environment or use default
    int port = 8080;
    const char* port_env = std::getenv("LOOM_API_PORT");
    if (port_env) {
        try {
            port = std::stoi(port_env);
        } catch (...) {
            std::cerr << "⚠️  Warning: Invalid LOOM_API_PORT value, using default 8080" << std::endl;
        }
    }

    std::cout << "\nInitializing Supabase client..." << std::endl;

    // Create Supabase database client
    auto db = std::make_shared<SupabaseClient>(supabase_key, supabase_url);
    std::cout << "✓ Supabase client initialized" << std::endl;

    // Create AuthManager for multi-tenant authentication
    std::cout << "Initializing AuthManager..." << std::endl;
    auto auth = std::make_shared<AuthManager>(db);
    std::cout << "✓ AuthManager initialized" << std::endl;

    // Create API server
    std::cout << "\nCreating API server..." << std::endl;
    ApiServer server(port, db, auth);

    std::cout << "\n════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Server ready!" << std::endl;
    std::cout << "════════════════════════════════════════════════════════════\n" << std::endl;

    // Start server (blocking)
    server.start();

    return 0;
}
