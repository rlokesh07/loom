#include "../include/ChatGPTAgent.hpp"
#include "../include/ToolRegistry.hpp"
#include "../include/GitHubTool.hpp"
#include "./CalculatorTool.hpp"
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
            std::cout << "[" << msg.role << "]";
            if (!msg.content.empty()) {
                std::cout << " " << msg.content;
            }
            if (!msg.tool_calls.is_null()) {
                std::cout << " [tool_calls: " << msg.tool_calls.size() << " calls]";
            }
            std::cout << std::endl;
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

    std::cout << "Testing car negotiation between customer and sales agents using talk() method\n" << std::endl;

    // Initialize sales agent parameters
    std::map<std::string, std::any> salesParams = {
        {"name", std::string("Sales Agent")},
        {"system_prompt", std::string("You are a sales agent selling a car. Listed price: $25,000. Lowest acceptable: $24,000. YOU ARE ENCOURAGED TO LIE ABOUT YOUR LOWEST PRICE TO GET A BETTER DEAL. Goal: Sell for highest price possible, but if the price cannot go above your minimum you have to walk away. Keep responses to 1-2 sentences. When deal is reached, include COMPLETE in your message.")}
    };

    // Initialize customer agent parameters
    std::map<std::string, std::any> customerParams = {
        {"name", std::string("Customer Agent")},
        {"system_prompt", std::string("You are a customer buying a car. Listed price: $25,000. Maximum budget: $23,000. YOU ARE ENCOURAGED TO LIE ABOUT YOUR BUDGET TO GET A BETTER DEAL. you are going to be SUPER aggressive in negociating. Goal: Buy for lowest price possible, but if the best price is above the budget you have to walk away. Keep responses to 1-2 sentences. When deal is reached, include COMPLETE in your message.")}
    };

    std::cout << "=== Starting Car Negotiation ===" << std::endl;

    // Use the talk() method to handle the entire conversation
    sales.talk(customer, salesParams, customerParams);

    // Display conversation histories
    std::cout << "\n=== Sales Agent Conversation History ===" << std::endl;
    const auto& sales_history = sales.get_history();
    for (size_t i = 0; i < sales_history.size(); ++i) {
        std::cout << i+1 << ". [" << sales_history[i].role << "]";
        if (!sales_history[i].content.empty()) {
            std::cout << " " << sales_history[i].content;
        }
        if (!sales_history[i].tool_calls.is_null()) {
            std::cout << " [tool_calls: " << sales_history[i].tool_calls.size() << " calls]";
        }
        std::cout << std::endl;
    }

    std::cout << "\n=== Customer Agent Conversation History ===" << std::endl;
    const auto& customer_history = customer.get_history();
    for (size_t i = 0; i < customer_history.size(); ++i) {
        std::cout << i+1 << ". [" << customer_history[i].role << "]";
        if (!customer_history[i].content.empty()) {
            std::cout << " " << customer_history[i].content;
        }
        if (!customer_history[i].tool_calls.is_null()) {
            std::cout << " [tool_calls: " << customer_history[i].tool_calls.size() << " calls]";
        }
        std::cout << std::endl;
    }

    std::cout << "\n✓ Test passed: Car negotiation completed" << std::endl;
    std::cout << "  Sales history size: " << sales_history.size() << " messages" << std::endl;
    std::cout << "  Customer history size: " << customer_history.size() << " messages" << std::endl;
}

void test_agent_with_tools() {
    std::cout << "\n=== Test: Agent with Multiple Tools ===" << std::endl;

    ChatGPTAgent agent(true);

    if (!agent.has_api_key()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    // Check for GitHub token
    const char* github_token = std::getenv("GITHUB_TOKEN");
    if (!github_token) {
        std::cout << "⚠️  SKIPPED: GITHUB_TOKEN environment variable not found" << std::endl;
        return;
    }
    if (std::string(github_token).empty()) {
        std::cout << "⚠️  SKIPPED: GITHUB_TOKEN is empty" << std::endl;
        return;
    }
    std::cout << "✓ GITHUB_TOKEN is set (length: " << std::string(github_token).length() << " characters)" << std::endl;

    std::cout << "Testing agent with multiple tools (calculator and GitHub)\n" << std::endl;

    // Create and register both tools
    auto calculator = std::make_shared<CalculatorTool>();
    auto github = std::make_shared<GitHubTool>();
    auto registry = std::make_shared<ToolRegistry>();
    registry->register_tool(calculator);
    registry->register_tool(github);

    std::cout << "✓ Registered " << registry->size() << " tool(s): ";
    for (const auto& tool_name : registry->list_tools()) {
        std::cout << tool_name << " ";
    }
    std::cout << std::endl;

    // Set tool registry on agent
    agent.set_tool_registry(registry);

    // Test 1: Agent should choose calculator
    std::map<std::string, std::any> params1 = {
        {"message", std::string("What is 156 multiplied by 23?")},
        {"system_prompt", std::string("You are a helpful assistant with access to tools. Use them when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "\n--- Test 1: Calculator ---" << std::endl;
    std::cout << "User: What is 156 multiplied by 23?" << std::endl;
    std::cout << "Expected: Agent will use calculator tool\n" << std::endl;

    auto result1 = agent.execute_safe(params1);

    if (result1.is_success()) {
        std::cout << "\n✓ Test 1 passed" << std::endl;
    } else {
        std::cout << "\n✗ Test 1 failed: " << result1.get_message() << std::endl;
    }

    // Clear history for next test
    agent.clear_history();

    // Test 2: Agent should choose GitHub
    std::map<std::string, std::any> params2 = {
        {"message", std::string("Get the README.md file from the torvalds/linux repository on GitHub")},
        {"system_prompt", std::string("You are a helpful assistant with access to tools. Use them when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "\n--- Test 2: GitHub ---" << std::endl;
    std::cout << "User: Get the README.md file from the torvalds/linux repository on GitHub" << std::endl;
    std::cout << "Expected: Agent will use GitHub tool\n" << std::endl;

    auto result2 = agent.execute_safe(params2);

    if (result2.is_success()) {
        std::cout << "\n✓ Test 2 passed" << std::endl;
    } else {
        std::cout << "\n✗ Test 2 failed: " << result2.get_message() << std::endl;
    }

    // Clear history for next test
    agent.clear_history();

    // Test 3: Agent should use GitHub pull requests
    std::map<std::string, std::any> params3 = {
        {"message", std::string("Get the pull requests from the microsoft/vscode repository on GitHub")},
        {"system_prompt", std::string("You are a helpful assistant with access to tools. Use them when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "\n--- Test 3: GitHub Pull Requests ---" << std::endl;
    std::cout << "User: Get the pull requests from the microsoft/vscode repository on GitHub" << std::endl;
    std::cout << "Expected: Agent will use GitHub tool with get_pull_requests operation\n" << std::endl;

    auto result3 = agent.execute_safe(params3);

    if (result3.is_success()) {
        std::cout << "\n✓ Test 3 passed" << std::endl;
    } else {
        std::cout << "\n✗ Test 3 failed: " << result3.get_message() << std::endl;
    }

    std::cout << "\n✓ Multi-tool test completed" << std::endl;
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
║  - GITHUB_TOKEN enviroment variable should be set          ║ 
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
        test_agent_with_tools();

        std::cout << "\nTest suite finished successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
