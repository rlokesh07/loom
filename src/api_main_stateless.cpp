#include "../include/ApiServer_stateless.hpp"
#include "../include/SupabaseClient.hpp"
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

    // Get client ID from environment or use default UUID
    std::string client_id = "00000000-0000-0000-0000-000000000000";  // Default UUID for testing
    const char* client_id_env = std::getenv("LOOM_CLIENT_ID");
    if (client_id_env) {
        client_id = client_id_env;
    }

    std::cout << "\nInitializing Supabase client..." << std::endl;

    // Create Supabase database client
    auto db = std::make_shared<SupabaseClient>(supabase_key, supabase_url);

    // Test database connection
    std::cout << "Testing database connection..." << std::endl;
    try {
        auto sessions = db->listClientSessions(client_id);
        std::cout << "✓ Database connection successful" << std::endl;
        std::cout << "  Found " << sessions.size() << " existing session(s) for client: " << client_id << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Database connection failed: " << e.what() << std::endl;
        std::cerr << "\nPlease check:" << std::endl;
        std::cerr << "  1. Your Supabase URL and API key are correct" << std::endl;
        std::cerr << "  2. The database schema has been applied (migrations/001_initial_schema.sql)" << std::endl;
        std::cerr << "  3. Your internet connection is working" << std::endl;
        return 1;
    }

    // Create API server
    std::cout << "\nCreating API server..." << std::endl;
    ApiServer server(port, db, client_id);

    std::cout << "\n════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Server ready!" << std::endl;
    std::cout << "════════════════════════════════════════════════════════════\n" << std::endl;

    // Start server (blocking)
    server.start();

    return 0;
}
