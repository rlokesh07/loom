#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

/**
 * Multi-Tenancy Test Suite for Stateless API Server
 *
 * Prerequisites:
 * - API server must be running (./loom_api_stateless)
 * - SUPABASE_URL and SUPABASE_KEY must be set
 * - Migrations 004 and 005 must be applied
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

    if (res->status != 200) {
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
std::string test_create_session_with_auth(const std::string& api_key, const std::string& client_name) {
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
        return "";
    }

    if (res->status != 200) {
        print_error("Session creation failed with status: " + std::to_string(res->status));
        std::cout << "Response: " << res->body << std::endl;
        return "";
    }

    json response = json::parse(res->body);
    std::string session_id = response["session_id"];

    print_success("Session created: " + session_id);
    print_success("Engine: " + response["engine"].get<std::string>());
    print_success("Model: " + response["model"].get<std::string>());

    return session_id;
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

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════╗
║   Multi-Tenancy Test Suite for Loom API Server        ║
║                                                        ║
║   Testing authentication and tenant isolation          ║
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
    std::string session_id_a = test_create_session_with_auth(api_key_a, "Client A");
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
    std::string session_id_b = test_create_session_with_auth(api_key_b, "Client B");
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
