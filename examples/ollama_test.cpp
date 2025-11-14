#include "../include/OllamaCompletionEngine.hpp"
#include <iostream>

int main() {
    try {
        auto engine = std::make_shared<OllamaCompletionEngine>();

        std::vector<ChatMessage> messages;
        messages.emplace_back("user", "Hello!");

        json request = engine->build_request_body(messages, "llama3.1:8b", 0.7, 100);

        std::cout << "Request JSON: " << request.dump(2) << std::endl;

        std::string response = engine->make_request(request);

        std::cout << "Response: " << response << std::endl;

        json full_response;
        std::string completion = engine->parse_completion_response(response, full_response);

        std::cout << "Completion: " << completion << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
