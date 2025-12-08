#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <cassert>

using json = nlohmann::json;

/**
 * Test suite for stateless API server
 *
 * Prerequisites:
 * - API server must be running (./api_main_stateless)
 * - SUPABASE_URL and SUPABASE_KEY must be set
 * - OPENAI_API_KEY must be set for ChatGPT tests
 * - Ollama server must be running for Ollama tests
 */

const std::string API_HOST = "localhost";
const int API_PORT = 8080;
const std::string API_BASE = "/api";

void print_test_header(const std::string& test_name) {
    std::cout << "\n=== Test: " << test_name << " ===" << std::endl;
}

void print_success(const std::string& message) {
    std::cout << "✓ " << message << std::endl;
}

void print_error(const std::string& message) {
    std::cout << "✗ " << message << std::endl;
}

void print_warning(const std::string& message) {
    std::cout << "⚠️  " << message << std::endl;
}

bool test_health_check() {
    print_test_header("Health Check");

    httplib::Client client(API_HOST, API_PORT);
    auto res = client.Get("/api/health");

    if (!res) {
        print_error("Failed to connect to API server");
        return false;
    }

    if (res->status != 200) {
        print_error("Health check failed with status: " + std::to_string(res->status));
        return false;
    }

    try {
        json response = json::parse(res->body);
        if (response["status"] != "ok") {
            print_error("Health check returned unexpected status");
            return false;
        }
    } catch (const std::exception& e) {
        print_error("Failed to parse health check response: " + std::string(e.what()));
        return false;
    }

    print_success("Health check passed");
    return true;
}

bool test_create_session() {
    print_test_header("Create Session");

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"engine", "chatgpt"},
        {"model", "gpt-3.5-turbo"},
        {"system_prompt", "You are a helpful assistant."}
    };

    auto res = client.Post("/api/sessions", request.dump(), "application/json");

    if (!res) {
        print_error("Failed to create session");
        return false;
    }

    if (res->status != 200) {
        print_error("Create session failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response.contains("session_id")) {
            print_error("Response missing session_id");
            return false;
        }

        std::string session_id = response["session_id"];
        print_success("Session created: " + session_id);

        if (response["engine"] != "chatgpt") {
            print_error("Engine mismatch");
            return false;
        }

        if (response["model"] != "gpt-3.5-turbo") {
            print_error("Model mismatch");
            return false;
        }

        print_success("Session metadata correct");
        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

std::string create_test_session(httplib::Client& client, const std::string& engine = "chatgpt", const std::string& model = "gpt-3.5-turbo") {
    json request = {
        {"engine", engine},
        {"model", model},
        {"system_prompt", "You are a helpful assistant. Keep responses brief."}
    };

    auto res = client.Post("/api/sessions", request.dump(), "application/json");

    if (!res || res->status != 200) {
        return "";
    }

    try {
        json response = json::parse(res->body);
        return response["session_id"];
    } catch (...) {
        return "";
    }
}

bool test_list_sessions() {
    print_test_header("List Sessions");

    httplib::Client client(API_HOST, API_PORT);

    // Create a test session first
    std::string session_id = create_test_session(client);
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    auto res = client.Get("/api/sessions");

    if (!res) {
        print_error("Failed to list sessions");
        return false;
    }

    if (res->status != 200) {
        print_error("List sessions failed with status: " + std::to_string(res->status));
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response.contains("sessions") || !response.contains("count")) {
            print_error("Response missing required fields");
            return false;
        }

        int count = response["count"];
        print_success("Found " + std::to_string(count) + " session(s)");

        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool test_get_session() {
    print_test_header("Get Session Details");

    httplib::Client client(API_HOST, API_PORT);

    // Create a test session
    std::string session_id = create_test_session(client);
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    auto res = client.Get("/api/sessions/" + session_id);

    if (!res) {
        print_error("Failed to get session");
        return false;
    }

    if (res->status != 200) {
        print_error("Get session failed with status: " + std::to_string(res->status));
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (response["session_id"] != session_id) {
            print_error("Session ID mismatch");
            return false;
        }

        print_success("Session details retrieved: " + session_id);
        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool test_chat_message() {
    print_test_header("Send Chat Message");

    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        print_warning("SKIPPED: OPENAI_API_KEY not set");
        return true;
    }

    httplib::Client client(API_HOST, API_PORT);
    client.set_read_timeout(60);  // Increase timeout for API calls

    // Create a test session
    std::string session_id = create_test_session(client, "chatgpt", "gpt-3.5-turbo");
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    print_success("Created session: " + session_id);

    // Send a message
    json request = {
        {"message", "Say 'Hello' in exactly one word."}
    };

    std::cout << "Sending message..." << std::endl;
    auto res = client.Post("/api/sessions/" + session_id + "/chat", request.dump(), "application/json");

    if (!res) {
        print_error("Failed to send message");
        return false;
    }

    if (res->status != 200) {
        print_error("Chat failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response["success"].get<bool>()) {
            print_error("Chat was not successful");
            if (response.contains("error")) {
                std::cout << "Error: " << response["error"] << std::endl;
            }
            return false;
        }

        std::string message = response["message"];
        print_success("Response received: " + message);

        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool test_conversation_history() {
    print_test_header("Conversation History");

    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        print_warning("SKIPPED: OPENAI_API_KEY not set");
        return true;
    }

    httplib::Client client(API_HOST, API_PORT);
    client.set_read_timeout(60);

    // Create a test session
    std::string session_id = create_test_session(client);
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    // Send two messages
    json request1 = {{"message", "What is 2+2?"}};
    auto res1 = client.Post("/api/sessions/" + session_id + "/chat", request1.dump(), "application/json");

    if (!res1 || res1->status != 200) {
        print_error("Failed to send first message");
        return false;
    }

    json request2 = {{"message", "What did I just ask you?"}};
    auto res2 = client.Post("/api/sessions/" + session_id + "/chat", request2.dump(), "application/json");

    if (!res2 || res2->status != 200) {
        print_error("Failed to send second message");
        return false;
    }

    // Get history
    auto res = client.Get("/api/sessions/" + session_id + "/history");

    if (!res || res->status != 200) {
        print_error("Failed to get history");
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response.contains("messages")) {
            print_error("Response missing messages field");
            return false;
        }

        int message_count = response["message_count"];
        print_success("History contains " + std::to_string(message_count) + " messages");

        // Should have at least 4 messages (2 user + 2 assistant)
        if (message_count < 4) {
            print_error("Expected at least 4 messages in history");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool test_register_tools() {
    print_test_header("Register Tools");

    httplib::Client client(API_HOST, API_PORT);

    // Create a test session
    std::string session_id = create_test_session(client);
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    // Get available tools first
    auto available_res = client.Get("/api/tools/available");
    if (!available_res || available_res->status != 200) {
        print_error("Failed to get available tools");
        return false;
    }

    print_success("Available tools retrieved");

    // Register some tools
    json request = {
        {"tools", json::array({"calculator", "base64"})}
    };

    auto res = client.Post("/api/sessions/" + session_id + "/tools", request.dump(), "application/json");

    if (!res || res->status != 200) {
        print_error("Failed to register tools");
        std::cout << "Response: " << res->body << std::endl;
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response.contains("registered_tools")) {
            print_error("Response missing registered_tools field");
            return false;
        }

        int tool_count = response["tool_count"];
        print_success("Registered " + std::to_string(tool_count) + " tools");

        return true;

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

bool test_delete_session() {
    print_test_header("Delete Session");

    httplib::Client client(API_HOST, API_PORT);

    // Create a test session
    std::string session_id = create_test_session(client);
    if (session_id.empty()) {
        print_error("Failed to create test session");
        return false;
    }

    print_success("Created session: " + session_id);

    // Delete it
    auto res = client.Delete("/api/sessions/" + session_id);

    if (!res || res->status != 200) {
        print_error("Failed to delete session");
        return false;
    }

    try {
        json response = json::parse(res->body);

        if (!response["success"].get<bool>()) {
            print_error("Delete was not successful");
            return false;
        }

        print_success("Session deleted");

        // Verify it's gone
        auto verify_res = client.Get("/api/sessions/" + session_id);
        if (verify_res && verify_res->status == 404) {
            print_success("Verified session no longer exists");
            return true;
        } else {
            print_error("Session still exists after deletion");
            return false;
        }

    } catch (const std::exception& e) {
        print_error("Failed to parse response: " + std::string(e.what()));
        return false;
    }
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║         Stateless API Server Test Suite                   ║
║                                                            ║
║  This test suite tests the stateless REST API endpoints.  ║
║                                                            ║
║  Prerequisites:                                            ║
║  - API server must be running (./api_main_stateless)       ║
║  - SUPABASE_URL and SUPABASE_KEY must be set               ║
║  - OPENAI_API_KEY for ChatGPT tests (optional)             ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    int passed = 0;
    int failed = 0;
    int total = 0;

    // Run all tests
    try {
        // Basic tests
        if (test_health_check()) passed++; else failed++;
        total++;

        if (test_create_session()) passed++; else failed++;
        total++;

        if (test_list_sessions()) passed++; else failed++;
        total++;

        if (test_get_session()) passed++; else failed++;
        total++;

        if (test_register_tools()) passed++; else failed++;
        total++;

        if (test_delete_session()) passed++; else failed++;
        total++;

        // Tests requiring OpenAI API key
        if (test_chat_message()) passed++; else failed++;
        total++;

        if (test_conversation_history()) passed++; else failed++;
        total++;

    } catch (const std::exception& e) {
        std::cout << "\n❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Print summary
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Test Summary                            ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Total:  " << total << " tests                                        ║" << std::endl;
    std::cout << "║  Passed: " << passed << " tests                                        ║" << std::endl;
    std::cout << "║  Failed: " << failed << " tests                                        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    if (failed == 0) {
        std::cout << "\n🎉 All tests passed!" << std::endl;
    } else {
        std::cout << "\n❌ Some tests failed!" << std::endl;
    }

    return failed > 0 ? 1 : 0;
}
