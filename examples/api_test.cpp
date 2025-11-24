#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Test for Loom API Server
 *
 * This tests the API endpoints via HTTP requests.
 * The server must be running before executing this test:
 *   ./loom_api
 */

static std::string base_url = "http://localhost:8080";

// Callback for curl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

// HTTP GET request
std::string http_get(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "GET failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return response;
}

// HTTP POST request with JSON body
std::string http_post(const std::string& url, const std::string& body) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "POST failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return response;
}

// HTTP DELETE request
std::string http_delete(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "DELETE failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    return response;
}

void test_health_endpoint() {
    std::cout << "=== Test: Health Endpoint ===" << std::endl;

    std::string response = http_get(base_url + "/api/health");
    std::cout << "Response: " << response << std::endl;

    json data = json::parse(response);
    if (data["status"] == "ok") {
        std::cout << "✓ Health check passed" << std::endl;
    } else {
        std::cout << "✗ Health check failed" << std::endl;
    }

    std::cout << std::endl;
}

void test_create_session() {
    std::cout << "=== Test: Create Session ===" << std::endl;

    json request_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string response = http_post(base_url + "/api/sessions", request_body.dump());
    std::cout << "Response: " << response << std::endl;

    if (!response.empty()) {
        json data = json::parse(response);
        if (data.contains("session_id")) {
            std::cout << "✓ Session created: " << data["session_id"] << std::endl;
        } else {
            std::cout << "✗ Session creation failed" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    std::cout << std::endl;
}

void test_chat_completion() {
    std::cout << "=== Test: Chat Completion ===" << std::endl;

    // First create a session
    json create_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string create_response = http_post(base_url + "/api/sessions", create_body.dump());

    if (create_response.empty()) {
        std::cout << "⚠️  Session endpoint not implemented yet" << std::endl;
        std::cout << std::endl;
        return;
    }

    json session_data = json::parse(create_response);
    std::string session_id = session_data["session_id"];

    // Now send a chat message
    json chat_body = {
        {"message", "Say 'Hello from API test' in exactly those words."},
        {"temperature", 0.7}
    };

    std::string chat_response = http_post(
        base_url + "/api/sessions/" + session_id + "/chat",
        chat_body.dump()
    );

    std::cout << "Response: " << chat_response << std::endl;

    if (!chat_response.empty()) {
        json data = json::parse(chat_response);
        if (data["success"] == true) {
            std::cout << "✓ Chat completion successful" << std::endl;
            std::cout << "  Message: " << data["message"] << std::endl;
        } else {
            std::cout << "✗ Chat completion failed" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    std::cout << std::endl;
}

void test_list_sessions() {
    std::cout << "=== Test: List Sessions ===" << std::endl;

    std::string response = http_get(base_url + "/api/sessions");
    std::cout << "Response: " << response << std::endl;

    if (!response.empty()) {
        json data = json::parse(response);
        if (data.contains("sessions")) {
            std::cout << "✓ Listed " << data["sessions"].size() << " sessions" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    std::cout << std::endl;
}

void test_delete_session() {
    std::cout << "=== Test: Delete Session ===" << std::endl;

    // First create a session
    json create_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string create_response = http_post(base_url + "/api/sessions", create_body.dump());

    if (create_response.empty()) {
        std::cout << "⚠️  Session endpoint not implemented yet" << std::endl;
        std::cout << std::endl;
        return;
    }

    json session_data = json::parse(create_response);
    std::string session_id = session_data["session_id"];

    // Now delete it
    std::string delete_response = http_delete(base_url + "/api/sessions/" + session_id);
    std::cout << "Response: " << delete_response << std::endl;

    if (!delete_response.empty()) {
        json data = json::parse(delete_response);
        if (data["success"] == true) {
            std::cout << "✓ Session deleted" << std::endl;
        } else {
            std::cout << "✗ Session deletion failed" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    std::cout << std::endl;
}

void test_available_tools() {
    std::cout << "=== Test: Available Tools ===" << std::endl;

    std::string response = http_get(base_url + "/api/tools/available");
    std::cout << "Response: " << response << std::endl;

    if (!response.empty()) {
        json data = json::parse(response);
        if (data.contains("tools")) {
            std::cout << "✓ Found " << data["tools"].size() << " available tools:" << std::endl;
            for (const auto& tool : data["tools"]) {
                std::cout << "  - " << tool["name"].get<std::string>() << ": "
                          << tool["description"].get<std::string>() << std::endl;
            }
        } else {
            std::cout << "✗ No tools array in response" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    std::cout << std::endl;
}

void test_register_tools() {
    std::cout << "=== Test: Register Tools ===" << std::endl;

    // First create a session
    json create_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string create_response = http_post(base_url + "/api/sessions", create_body.dump());

    if (create_response.empty()) {
        std::cout << "⚠️  Session endpoint not implemented yet" << std::endl;
        std::cout << std::endl;
        return;
    }

    json session_data = json::parse(create_response);
    std::string session_id = session_data["session_id"];
    std::cout << "Created session: " << session_id << std::endl;

    // Check initial tools (should be empty)
    std::string initial_tools = http_get(base_url + "/api/sessions/" + session_id + "/tools");
    json initial_data = json::parse(initial_tools);
    std::cout << "Initial tools: " << initial_data["tool_count"] << std::endl;

    // Register some tools
    json register_body = {
        {"tools", {"calculator", "base64", "web_search"}}
    };

    std::string register_response = http_post(
        base_url + "/api/sessions/" + session_id + "/tools",
        register_body.dump()
    );
    std::cout << "Register response: " << register_response << std::endl;

    if (!register_response.empty()) {
        json data = json::parse(register_response);
        if (data.contains("registered_tools")) {
            std::cout << "✓ Registered " << data["tool_count"] << " tools" << std::endl;
            for (const auto& tool : data["registered_tools"]) {
                std::cout << "  - " << tool.get<std::string>() << std::endl;
            }
        } else {
            std::cout << "✗ Registration failed" << std::endl;
        }
    } else {
        std::cout << "⚠️  No response - endpoint not implemented yet" << std::endl;
    }

    // Verify tools are registered
    std::string tools_response = http_get(base_url + "/api/sessions/" + session_id + "/tools");
    json tools_data = json::parse(tools_response);
    std::cout << "Session now has " << tools_data["tool_count"] << " tools" << std::endl;

    // Test registering invalid tool
    json invalid_body = {
        {"tools", {"nonexistent_tool", "calculator"}}
    };
    std::string invalid_response = http_post(
        base_url + "/api/sessions/" + session_id + "/tools",
        invalid_body.dump()
    );
    json invalid_data = json::parse(invalid_response);
    if (invalid_data.contains("failed_tools")) {
        std::cout << "✓ Correctly identified invalid tools: ";
        for (const auto& tool : invalid_data["failed_tools"]) {
            std::cout << tool.get<std::string>() << " ";
        }
        std::cout << std::endl;
    }

    // Clean up
    http_delete(base_url + "/api/sessions/" + session_id);

    std::cout << std::endl;
}

void test_unregister_tool() {
    std::cout << "=== Test: Unregister Tool ===" << std::endl;

    // Create a session
    json create_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string create_response = http_post(base_url + "/api/sessions", create_body.dump());

    if (create_response.empty()) {
        std::cout << "⚠️  Session endpoint not implemented yet" << std::endl;
        std::cout << std::endl;
        return;
    }

    json session_data = json::parse(create_response);
    std::string session_id = session_data["session_id"];

    // Register tools
    json register_body = {
        {"tools", {"calculator", "base64"}}
    };
    http_post(base_url + "/api/sessions/" + session_id + "/tools", register_body.dump());

    // Verify initial count
    std::string initial_tools = http_get(base_url + "/api/sessions/" + session_id + "/tools");
    json initial_data = json::parse(initial_tools);
    std::cout << "Initial tool count: " << initial_data["tool_count"] << std::endl;

    // Unregister a tool
    std::string delete_response = http_delete(
        base_url + "/api/sessions/" + session_id + "/tools/calculator"
    );
    std::cout << "Delete response: " << delete_response << std::endl;

    if (!delete_response.empty()) {
        json data = json::parse(delete_response);
        if (data["success"] == true) {
            std::cout << "✓ Tool unregistered: " << data["removed_tool"] << std::endl;
        } else {
            std::cout << "✗ Unregister failed" << std::endl;
        }
    }

    // Verify tool count decreased
    std::string final_tools = http_get(base_url + "/api/sessions/" + session_id + "/tools");
    json final_data = json::parse(final_tools);
    std::cout << "Final tool count: " << final_data["tool_count"] << std::endl;

    if (final_data["tool_count"] == 1) {
        std::cout << "✓ Tool count correctly decreased" << std::endl;
    } else {
        std::cout << "✗ Tool count mismatch" << std::endl;
    }

    // Test unregistering non-existent tool
    std::string invalid_delete = http_delete(
        base_url + "/api/sessions/" + session_id + "/tools/nonexistent"
    );
    json invalid_data = json::parse(invalid_delete);
    if (invalid_data.contains("error") && invalid_data["error"]["code"] == "TOOL_NOT_FOUND") {
        std::cout << "✓ Correctly returned TOOL_NOT_FOUND for invalid tool" << std::endl;
    }

    // Clean up
    http_delete(base_url + "/api/sessions/" + session_id);

    std::cout << std::endl;
}

void test_register_agent_as_tool() {
    std::cout << "=== Test: Register Agent as Tool ===" << std::endl;

    // Create source agent (researcher)
    json researcher_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string researcher_response = http_post(base_url + "/api/sessions", researcher_body.dump());

    if (researcher_response.empty()) {
        std::cout << "⚠️  Session endpoint not implemented yet" << std::endl;
        std::cout << std::endl;
        return;
    }

    json researcher_data = json::parse(researcher_response);
    std::string researcher_id = researcher_data["session_id"];
    std::cout << "Created researcher session: " << researcher_id << std::endl;

    // Give researcher some tools
    json tools_body = {
        {"tools", {"web_search", "calculator"}}
    };
    http_post(base_url + "/api/sessions/" + researcher_id + "/tools", tools_body.dump());

    // Create main agent
    json main_body = {
        {"engine", "chatgpt"},
        {"model", "gpt-4o-mini"},
        {"verbose", false}
    };

    std::string main_response = http_post(base_url + "/api/sessions", main_body.dump());
    json main_data = json::parse(main_response);
    std::string main_id = main_data["session_id"];
    std::cout << "Created main session: " << main_id << std::endl;

    // Register researcher as tool for main agent
    json register_body = {
        {"source_session_id", researcher_id},
        {"tool_name", "researcher"},
        {"tool_description", "Agent that can search and calculate"}
    };

    std::string register_response = http_post(
        base_url + "/api/sessions/" + main_id + "/tools/agent",
        register_body.dump()
    );
    std::cout << "Register response: " << register_response << std::endl;

    if (!register_response.empty()) {
        json data = json::parse(register_response);
        if (data["success"] == true) {
            std::cout << "✓ Agent registered as tool: " << data["registered_agent_tool"] << std::endl;
        } else {
            std::cout << "✗ Agent registration failed" << std::endl;
        }
    }

    // Verify agent tool appears in session tools
    std::string tools_response = http_get(base_url + "/api/sessions/" + main_id + "/tools");
    json tools_data = json::parse(tools_response);
    std::cout << "Main session now has " << tools_data["tool_count"] << " tools" << std::endl;

    bool found_researcher = false;
    for (const auto& tool : tools_data["tools"]) {
        if (tool["name"] == "researcher") {
            found_researcher = true;
            std::cout << "✓ Found researcher tool in session" << std::endl;
            break;
        }
    }
    if (!found_researcher) {
        std::cout << "✗ Researcher tool not found in session" << std::endl;
    }

    // Test self-registration prevention
    json self_body = {
        {"source_session_id", main_id}
    };
    std::string self_response = http_post(
        base_url + "/api/sessions/" + main_id + "/tools/agent",
        self_body.dump()
    );
    json self_data = json::parse(self_response);
    if (self_data.contains("error") && self_data["error"]["code"] == "SELF_REGISTRATION") {
        std::cout << "✓ Correctly prevented self-registration" << std::endl;
    }

    // Test duplicate tool name prevention
    json dup_body = {
        {"source_session_id", researcher_id},
        {"tool_name", "researcher"}
    };
    std::string dup_response = http_post(
        base_url + "/api/sessions/" + main_id + "/tools/agent",
        dup_body.dump()
    );
    json dup_data = json::parse(dup_response);
    if (dup_data.contains("error") && dup_data["error"]["code"] == "TOOL_ALREADY_EXISTS") {
        std::cout << "✓ Correctly prevented duplicate tool registration" << std::endl;
    }

    // Clean up
    http_delete(base_url + "/api/sessions/" + main_id);
    http_delete(base_url + "/api/sessions/" + researcher_id);

    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    // Allow custom base URL
    if (argc > 1) {
        base_url = argv[1];
    }

    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║              Loom API Integration Test                     ║
║                                                            ║
║  Make sure the API server is running:                      ║
║    ./loom_api                                              ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    std::cout << "Testing against: " << base_url << std::endl << std::endl;

    curl_global_init(CURL_GLOBAL_ALL);

    try {
        test_health_endpoint();
        test_create_session();
        test_list_sessions();
        test_chat_completion();
        test_delete_session();
        test_available_tools();
        test_register_tools();
        test_unregister_tool();
        test_register_agent_as_tool();

        std::cout << "All tests completed!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ ERROR: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
