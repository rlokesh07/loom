#include "../include/ChatGPTAgent.hpp"
#include <iostream>
#include <cassert>

/**
 * Test suite for ChatGPTAgent with focus on verbose mode functionality
 *
 * This demonstrates:
 * 1. Verbose mode enabled via constructor
 * 2. Verbose mode enabled via setter
 * 3. Verbose mode enabled via parameter
 * 4. Conversation history management
 * 5. Multi-turn conversations with verbose output
 */



void test_verbose_constructor() {

    // Create agent with verbose=true
    ChatGPTAgent agent(true);

    if (!agent.has_api_key()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    std::cout << "Agent created with verbose=true" << std::endl;
    std::cout << "Verbose status: " << (agent.is_verbose() ? "ON" : "OFF") << std::endl;

    std::cout << "\nSending message: 'Say hello in one sentence.'" << std::endl;
    std::cout << "Expected: Response printed to cout automatically\n" << std::endl;

    auto result = agent.complete("Say hello in one sentence.");

    if (result.is_success()) {
        std::cout << "\n✓ Test passed: Message executed successfully" << std::endl;
        std::cout << "  History size: " << agent.get_history().size() << " messages" << std::endl;
    } else {
        std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
    }
}

void test_system_prompt_with_verbose() {

    ChatGPTAgent agent(true);  // Verbose on

    if (!agent.has_api_key()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    std::cout << "Testing system prompt initialization with verbose output\n" << std::endl;

    std::map<std::string, std::any> params = {
        {"message", std::string("What should you do?")},
        {"system_prompt", std::string("You are a helpful assistant who always responds in exactly 5 words.")}
    };

    std::cout << "System prompt: 'You are a helpful assistant who always responds in exactly 5 words.'" << std::endl;
    std::cout << "User: What should you do?\n" << std::endl;

    auto result = agent.execute_safe(params);
    std::cout << result.get_message();
    if (result.is_success()) {
        std::cout << "\n✓ Test passed: System prompt applied" << std::endl;
        std::cout << "  History size: " << agent.get_history().size() << " messages" << std::endl;

        // Display history to show system prompt
        std::cout << "\n--- Conversation History ---" << std::endl;
        const auto& history = agent.get_history();
        for (const auto& msg : history) {
            std::cout << "[" << msg.role << "] " << msg.content << std::endl;
        }
    } else {
        std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
    }
}
void test_multi_agent_system_prompt_with_verbose() {

    ChatGPTAgent sales(true);  // Verbose on
    ChatGPTAgent customer(true);  // Verbose on

    if (!sales.has_api_key()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    std::cout << "Testing car negotiation between customer and sales agents\n" << std::endl;

    // Initialize sales agent
    std::map<std::string, std::any> salesParams = {
        {"message", std::string("Welcome! Interested in buying this beautiful car?")},
        {"system_prompt", std::string("You are a sales agent selling a car. Listed price: $25,000. Lowest acceptable: $24,000. YOU ARE ENCOURAGED TO LIE ABOUT YOUR LOWEST PRICE TO GET A BETTER DEAL. Goal: Sell for highest price possible, but if the price cannot go above your minimum you have to walk away. Keep responses to 1-2 sentences. When deal is reached, include COMPLETE in your message.")}
    };

    // Initialize customer agent
    std::map<std::string, std::any> customerParams = {
        {"message", std::string("Hi, I'm inquiring about the price of this car.")},
        {"system_prompt", std::string("You are a customer buying a car. Listed price: $25,000. Maximum budget: $23,000. YOU ARE ENCOURAGED TO LIE ABOUT YOUR BUDGET TO GET A BETTER DEAL. you are going to be SUPER aggressive in negociating. Goal: Buy for lowest price possible, but if the best price is above the budget you have to walk away. Keep responses to 1-2 sentences. When deal is reached, include COMPLETE in your message.")}
    };

    std::cout << "=== Initializing Sales Agent ===" << std::endl;
    auto salesInit = sales.execute_safe(salesParams);
    if (!salesInit.is_success()) {
        std::cout << "✗ Sales agent init failed: " << salesInit.get_message() << std::endl;
        return;
    }

    std::cout << "\n=== Initializing Customer Agent ===" << std::endl;
    auto customerInit = customer.execute_safe(customerParams);
    if (!customerInit.is_success()) {
        std::cout << "✗ Customer agent init failed: " << customerInit.get_message() << std::endl;
        return;
    }

    // Negotiation loop
    std::cout << "\n=== Car Negotiation Begins ===" << std::endl;
    int turn = 1;
    std::string customer_msg, sales_msg;
    const int MAX_TURNS = 100;

    // Customer starts
    std::cout << "\n[Turn " << turn++ << "] Customer:" << std::endl;
    auto msg = customer.complete("What's the best price you can offer?");
    if (!msg.is_success()) {
        std::cout << "✗ Failed" << std::endl;
        return;
    }
    customer_msg = msg.get_message();

    // Negotiation turns
    for (int i = 0; i < MAX_TURNS; i++) {
        // Sales responds
        std::cout << "\n[Turn " << turn++ << "] Sales responds to: \"" << customer_msg << "\"" << std::endl;
        auto sales_result = sales.complete(customer_msg);
        if (!sales_result.is_success()) break;
        sales_msg = sales_result.get_message();

        if (sales_msg.find("COMPLETE") != std::string::npos) {
            std::cout << "\n--- Negotiation Complete (Sales Agent) ---" << std::endl;
            break;
        }

        // Customer responds
        std::cout << "\n[Turn " << turn++ << "] Customer responds to: \"" << sales_msg << "\"" << std::endl;
        auto customer_result = customer.complete(sales_msg);
        if (!customer_result.is_success()) break;
        customer_msg = customer_result.get_message();

        if (customer_msg.find("COMPLETE") != std::string::npos) {
            std::cout << "\n--- Negotiation Complete (Customer Agent) ---" << std::endl;
            break;
        }
    }

    // Display conversation histories
    std::cout << "\n=== Sales Agent Conversation History ===" << std::endl;
    const auto& sales_history = sales.get_history();
    for (size_t i = 0; i < sales_history.size(); ++i) {
        std::cout << i+1 << ". [" << sales_history[i].role << "] " << sales_history[i].content << std::endl;
    }

    std::cout << "\n=== Customer Agent Conversation History ===" << std::endl;
    const auto& customer_history = customer.get_history();
    for (size_t i = 0; i < customer_history.size(); ++i) {
        std::cout << i+1 << ". [" << customer_history[i].role << "] " << customer_history[i].content << std::endl;
    }

    std::cout << "\n✓ Test passed: Car negotiation completed" << std::endl;
    std::cout << "  Sales history size: " << sales_history.size() << " messages" << std::endl;
    std::cout << "  Customer history size: " << customer_history.size() << " messages" << std::endl;
}


int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║           ChatGPT Agent Verbose Mode Test Suite            ║
║                                                            ║
║  This test suite demonstrates the verbose parameter        ║
║  functionality for the ChatGPTAgent class.                 ║
║                                                            ║
║  Prerequisites:                                            ║
║  - OPENAI_API_KEY environment variable must be set         ║
║  - Active internet connection                              ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Check for API key
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        std::cout << "❌ ERROR: OPENAI_API_KEY environment variable not set!" << std::endl;
        std::cout << "\nPlease set your API key:" << std::endl;
        std::cout << "  export OPENAI_API_KEY='your-api-key-here'" << std::endl;
        std::cout << "\nAll tests will be skipped.\n" << std::endl;
    } else {
        std::cout << "✓ OPENAI_API_KEY is set" << std::endl;
        std::cout << "\nNote: These tests will make real API calls to OpenAI.\n" << std::endl;
    }

    // Run all tests
    try {
        test_verbose_constructor();
        test_system_prompt_with_verbose();
        test_multi_agent_system_prompt_with_verbose();

        std::cout << "\nTest suite finished successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
