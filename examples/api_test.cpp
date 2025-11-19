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

        std::cout << "All tests completed!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ ERROR: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
