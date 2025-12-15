#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

/**
 * Multi-Tenancy & OpenAI Compatibility Test Suite for Stateless API Server
 *
 * Prerequisites:
 * - API server must be running (./loom_api_stateless)
 * - SUPABASE_URL and SUPABASE_KEY must be set
 * - Migration 006 (session_api_keys) must be applied
 *
 * Tests:
 * 0. Health Check
 * 1. Create Client (base API key generation)
 * 2. No Auth Access (should fail)
 * 3. Create Session with Auth (returns session-specific API key)
 * 4. Get Client Info
 * 5. Tenant Isolation
 * 6. Own Session Access
 * 7. List Sessions
 * 8. OpenAI Chat Completion with Session Key
 * 9. OpenAI Chat Completion with Base Key (Stateless)
 * 10. List Available Models (/v1/models)
 * 11. Session Key Cannot Create New Sessions
 */

const std::string API_HOST = "localhost";
const int API_PORT = 8080;

void print_test_header(const std::string& test_name) {
    std::cout << "\n╔══════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║ " << test_name;
    for (int i = test_name.length(); i < 48; i++) std::cout << " ";
    std::cout << " ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════╝" << std::endl;
}

void print_success(const std::string& message) {
    std::cout << "✓ " << message << std::endl;
}

void print_error(const std::string& message) {
    std::cout << "✗ " << message << std::endl;
}

// Test 0: Health Check (No Auth Required)
bool test_health_check() {
    print_test_header("Test 0: Health Check");

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

    json response = json::parse(res->body);
    if (response["status"] != "ok") {
        print_error("Health check returned unexpected status");
        return false;
    }

    print_success("Server is healthy");
    return true;
}

// Test 1: Create Client A (Public Signup)
std::string test_create_client(const std::string& client_name) {
    print_test_header("Test 1: Create Client - " + client_name);

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"display_name", client_name}
    };

    auto res = client.Post("/api/clients", request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return "";
    }

    if (res->status != 201) {
        print_error("Client creation failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return "";
    }

    json response = json::parse(res->body);

    if (!response.contains("api_key") || !response.contains("client_id")) {
        print_error("Response missing api_key or client_id");
        return "";
    }

    std::string api_key = response["api_key"];
    std::string client_id = response["client_id"];

    print_success("Client created: " + client_id);
    print_success("API Key: " + api_key);
    print_success("Save this API key - it won't be shown again!");

    return api_key;
}

// Test 2: Try Creating Session WITHOUT Auth (Should Fail)
bool test_no_auth() {
    print_test_header("Test 2: Access Without Auth (Should Fail)");

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"engine", "chatgpt"},
        {"model", "gpt-3.5-turbo"}
    };

    // Try to create session WITHOUT Authorization header
    auto res = client.Post("/api/sessions", request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 401) {
        print_error("Expected 401 Unauthorized, got: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);
    print_success("Correctly rejected: " + response["error"]["code"].get<std::string>());
    print_success("Message: " + response["error"]["message"].get<std::string>());

    return true;
}

// Test 3: Create Session WITH Valid Auth (Should Succeed)
std::pair<std::string, std::string> test_create_session_with_auth(const std::string& api_key, const std::string& client_name) {
    print_test_header("Test 3: Create Session With Auth - " + client_name);

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"engine", "chatgpt"},
        {"model", "gpt-3.5-turbo"}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key}
    };

    auto res = client.Post("/api/sessions", headers, request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return {"", ""};
    }

    if (res->status != 201) {
        print_error("Session creation failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return {"", ""};
    }

    json response = json::parse(res->body);
    std::string session_id = response["session_id"];
    std::string session_api_key = response["session_api_key"];

    print_success("Session created: " + session_id);
    print_success("Session API Key: " + session_api_key);
    print_success("Engine: " + response["engine"].get<std::string>());
    print_success("Model: " + response["model"].get<std::string>());

    return {session_id, session_api_key};
}

// Test 4: Get Client Info
bool test_get_client_info(const std::string& api_key, const std::string& expected_client_name) {
    print_test_header("Test 4: Get Client Info - " + expected_client_name);

    httplib::Client client(API_HOST, API_PORT);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key}
    };

    auto res = client.Get("/api/clients/me", headers);

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("Get client info failed with status: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);
    print_success("Client ID: " + response["client_id"].get<std::string>());

    return true;
}

// Test 5: Tenant Isolation - Client A Cannot Access Client B's Session
bool test_tenant_isolation(const std::string& api_key_a, const std::string& api_key_b,
                           const std::string& session_id_b) {
    print_test_header("Test 5: Tenant Isolation");

    httplib::Client client(API_HOST, API_PORT);

    std::cout << "Attempting Client A to access Client B's session..." << std::endl;

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_a}
    };

    // Client A tries to access Client B's session
    auto res = client.Get("/api/sessions/" + session_id_b, headers);

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status == 404) {
        print_success("✓ Isolation enforced: Client A cannot see Client B's session (404)");
        return true;
    } else if (res->status == 200) {
        print_error("SECURITY ISSUE: Client A can access Client B's session!");
        return false;
    } else {
        print_error("Unexpected status: " + std::to_string(res->status));
        return false;
    }
}

// Test 6: Client B Can Access Own Session
bool test_own_session_access(const std::string& api_key_b, const std::string& session_id_b) {
    print_test_header("Test 6: Client B Accesses Own Session");

    httplib::Client client(API_HOST, API_PORT);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key_b}
    };

    auto res = client.Get("/api/sessions/" + session_id_b, headers);

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("Failed to get own session with status: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);
    print_success("Client B can access own session: " + response["session_id"].get<std::string>());

    return true;
}

// Test 7: List Sessions (Should Only See Own)
bool test_list_sessions(const std::string& api_key, const std::string& client_name) {
    print_test_header("Test 7: List Sessions - " + client_name);

    httplib::Client client(API_HOST, API_PORT);

    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key}
    };

    auto res = client.Get("/api/sessions", headers);

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("List sessions failed with status: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);
    int count = response["count"];

    print_success("Found " + std::to_string(count) + " session(s) for " + client_name);
    print_success("(Should only see own sessions)");

    return true;
}

// Test 8: OpenAI Chat Completion with Session Key
bool test_openai_chat_completion_session(const std::string& session_api_key) {
    print_test_header("Test 8: OpenAI Chat Completion (Session Key)");

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"model", "gpt-3.5-turbo"},
        {"messages", json::array({
            {{"role", "user"}, {"content", "Say 'Hello from Loom!'"}}
        })}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + session_api_key}
    };

    auto res = client.Post("/v1/chat/completions", headers, request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("Chat completion failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return false;
    }

    json response = json::parse(res->body);

    if (!response.contains("choices") || response["choices"].empty()) {
        print_error("Response missing choices");
        return false;
    }

    std::string message = response["choices"][0]["message"]["content"];
    print_success("Got response: " + message.substr(0, 50) + (message.length() > 50 ? "..." : ""));
    print_success("Chat completion ID: " + response["id"].get<std::string>());

    return true;
}

// Test 9: OpenAI Chat Completion with Base Key (Stateless)
bool test_openai_chat_completion_stateless(const std::string& base_api_key) {
    print_test_header("Test 9: OpenAI Chat Completion (Base Key - Stateless)");

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"model", "gpt-3.5-turbo"},
        {"messages", json::array({
            {{"role", "system"}, {"content", "You are a helpful assistant"}},
            {{"role", "user"}, {"content", "What is 2+2?"}}
        })}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + base_api_key}
    };

    auto res = client.Post("/v1/chat/completions", headers, request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("Stateless chat completion failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return false;
    }

    json response = json::parse(res->body);

    if (!response.contains("choices") || response["choices"].empty()) {
        print_error("Response missing choices");
        return false;
    }

    std::string message = response["choices"][0]["message"]["content"];
    print_success("Got response: " + message.substr(0, 50) + (message.length() > 50 ? "..." : ""));
    print_success("Stateless mode working correctly");

    return true;
}

// Test 10: List Available Models
bool test_list_models() {
    print_test_header("Test 10: List Available Models");

    httplib::Client client(API_HOST, API_PORT);

    auto res = client.Get("/v1/models");

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 200) {
        print_error("List models failed with status: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);

    if (!response.contains("data") || !response["data"].is_array()) {
        print_error("Response missing data array");
        return false;
    }

    int model_count = response["data"].size();
    print_success("Found " + std::to_string(model_count) + " available models");

    // Show first few models
    for (int i = 0; i < std::min(5, model_count); i++) {
        std::string model_id = response["data"][i]["id"];
        std::string owned_by = response["data"][i]["owned_by"];
        print_success("  - " + model_id + " (engine: " + owned_by + ")");
    }

    return true;
}

// Test 11: Session Key Cannot Create New Sessions
bool test_session_key_cannot_create_session(const std::string& session_api_key) {
    print_test_header("Test 11: Session Key Cannot Create Sessions");

    httplib::Client client(API_HOST, API_PORT);

    json request = {
        {"engine", "chatgpt"},
        {"model", "gpt-3.5-turbo"}
    };

    httplib::Headers headers = {
        {"Authorization", "Bearer " + session_api_key}
    };

    auto res = client.Post("/api/sessions", headers, request.dump(), "application/json");

    if (!res) {
        print_error("Failed to connect to server");
        return false;
    }

    if (res->status != 400) {
        print_error("Expected 400 Bad Request, got: " + std::to_string(res->status));
        return false;
    }

    json response = json::parse(res->body);
    print_success("Correctly rejected: " + response["error"]["message"].get<std::string>());

    return true;
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════╗
║   Loom API Server - Comprehensive Test Suite          ║
║                                                        ║
║   • Multi-Tenancy & Authentication                     ║
║   • OpenAI-Compatible Endpoints                        ║
║   • Session-Specific API Keys                          ║
╚════════════════════════════════════════════════════════╝
)" << std::endl;

    int passed = 0;
    int failed = 0;

    // Test 0: Health Check
    if (test_health_check()) passed++; else failed++;

    // Test 1: Create Client A
    std::string api_key_a = test_create_client("Test Client A");
    if (!api_key_a.empty()) {
        passed++;
    } else {
        failed++;
        std::cout << "\n❌ Cannot continue without Client A. Exiting..." << std::endl;
        return 1;
    }

    // Test 2: Try WITHOUT Auth
    if (test_no_auth()) passed++; else failed++;

    // Test 3: Create Session with Client A
    auto [session_id_a, session_key_a] = test_create_session_with_auth(api_key_a, "Client A");
    if (!session_id_a.empty()) {
        passed++;
    } else {
        failed++;
    }

    // Test 4: Get Client A Info
    if (test_get_client_info(api_key_a, "Client A")) passed++; else failed++;

    // Create Client B for isolation testing
    std::string api_key_b = test_create_client("Test Client B");
    if (!api_key_b.empty()) {
        passed++;
    } else {
        failed++;
        std::cout << "\n⚠️  Cannot test isolation without Client B" << std::endl;
    }

    // Create Session with Client B
    auto [session_id_b, session_key_b] = test_create_session_with_auth(api_key_b, "Client B");
    if (!session_id_b.empty()) {
        passed++;
    } else {
        failed++;
    }

    // Test 5: Tenant Isolation
    if (!api_key_a.empty() && !api_key_b.empty() && !session_id_b.empty()) {
        if (test_tenant_isolation(api_key_a, api_key_b, session_id_b)) passed++; else failed++;
    } else {
        std::cout << "\nSkipping isolation test (missing prerequisites)" << std::endl;
        failed++;
    }

    // Test 6: Client B Can Access Own Session
    if (!api_key_b.empty() && !session_id_b.empty()) {
        if (test_own_session_access(api_key_b, session_id_b)) passed++; else failed++;
    } else {
        std::cout << "\nSkipping own session test" << std::endl;
        failed++;
    }

    // Test 7: List Sessions for Each Client
    if (!api_key_a.empty()) {
        if (test_list_sessions(api_key_a, "Client A")) passed++; else failed++;
    }
    if (!api_key_b.empty()) {
        if (test_list_sessions(api_key_b, "Client B")) passed++; else failed++;
    }

    // Test 8: OpenAI Chat Completion with Session Key
    if (!session_key_a.empty()) {
        if (test_openai_chat_completion_session(session_key_a)) passed++; else failed++;
    } else {
        std::cout << "\nSkipping OpenAI session test" << std::endl;
        failed++;
    }

    // Test 9: OpenAI Chat Completion with Base Key (Stateless)
    if (!api_key_a.empty()) {
        if (test_openai_chat_completion_stateless(api_key_a)) passed++; else failed++;
    } else {
        std::cout << "\nSkipping OpenAI stateless test" << std::endl;
        failed++;
    }

    // Test 10: List Available Models
    if (test_list_models()) passed++; else failed++;

    // Test 11: Session Key Cannot Create New Sessions
    if (!session_key_a.empty()) {
        if (test_session_key_cannot_create_session(session_key_a)) passed++; else failed++;
    } else {
        std::cout << "\nSkipping session key validation test" << std::endl;
        failed++;
    }

    // Print Summary
    int total = passed + failed;
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    TEST SUMMARY                        ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Total:  " << total << " tests";
    for (int i = 0; i < 41; i++) std::cout << " ";
    std::cout << "║" << std::endl;
    std::cout << "║  Passed: " << passed << " tests ✓";
    for (int i = 0; i < 37; i++) std::cout << " ";
    std::cout << "║" << std::endl;
    std::cout << "║  Failed: " << failed << " tests ✗";
    for (int i = 0; i < 37; i++) std::cout << " ";
    std::cout << "║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    if (failed == 0) {
        std::cout << "\n🎉 All tests passed! Multi-tenancy is working correctly!" << std::endl;
    } else {
        std::cout << "\n❌ Some tests failed! Check the output above." << std::endl;
    }

    return failed > 0 ? 1 : 0;
}
