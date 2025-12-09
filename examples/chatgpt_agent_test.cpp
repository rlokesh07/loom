#include "../include/Agent.hpp"
#include "../include/OllamaCompletionEngine.hpp"
#include "../include/GeminiCompletionEngine.hpp"
#include "../include/ToolRegistry.hpp"
#include "../include/GitHubTool.hpp"
#include "../include/WebSearchTool.hpp"
#include "../include/Base64Tool.hpp"
#include "../include/WebFetchTool.hpp"
#include "./CalculatorTool.hpp"
#include <iostream>
#include <cassert>
#include <memory>

/**
 * Test suite for Agent with focus on verbose mode functionality
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
    Agent agent(true);

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

    Agent agent(true);  // Verbose on

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

    Agent sales(true);  // Verbose on
    Agent customer(true);  // Verbose on

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


    Agent agent(true);
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

    // Create and register tools using template method
    auto registry = std::make_shared<ToolRegistry>();

    registry->register_tool<CalculatorTool>();
    registry->register_tool<GitHubTool>();
    registry->register_tool<Base64Tool>();

    auto cleanCodeAgent = registry->register_tool<Agent>(true);
    cleanCodeAgent->set_name("cleanCodeAgent");
    cleanCodeAgent->set_description("agent in charge of making sure everything is up to clean code standards");
    cleanCodeAgent->add_message("system", "Make sure everything is according to there rules Clean code principles from Uncle Bob's Clean Code emphasize writing code primarily for human readability and maintainability. Use meaningful, pronounceable names for variables and functions that clearly reveal their intent and purpose while avoiding cryptic abbreviations. Keep functions small (ideally 5-20 lines) with each function doing one thing only, minimizing arguments to 0-2 when possible, and using descriptive names that indicate their action. Apply the Single Responsibility Principle where each class or module has one reason to change, maintain clear separation of concerns, and structure code hierarchically with high-level abstractions at the top and details below. Prefer self-documenting code over comments, using comments only to explain why rather than what, and always remove commented-out or obsolete code. Handle errors using exceptions rather than return codes, avoid returning null by using Optional or empty objects instead, and keep error handling separate from business logic. Follow the Boy Scout Rule by leaving code cleaner than you found it, refactor continuously, and always remember that code is read ten times more than it is written, so optimize for readability above all else.");


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
        {"message", std::string("Get the pull requests from the microsoft/vscode repository on GitHub. Find the most technical sounding one and look at its details. Make sure that all the files are following clean code principles by passing the actual code from the github to the clean code agent")},
        {"system_prompt", std::string("You are a helpful assistant with access to tools. Use them when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "\n--- Test 3: GitHub Pull Requests ---" << std::endl;
    std::cout << "User: Get the pull requests from the microsoft/vscode repository on GitHub and find one it likes, then call the pull request details call on it" << std::endl;
    std::cout << "Expected: Agent will use GitHub tool with get_pull_requests operation and get_pull_request_details operation\n" << std::endl;

    auto result3 = agent.execute_safe(params3);

    if (result3.is_success()) {
        std::cout << "\n✓ Test 3 passed" << std::endl;
    } else {
        std::cout << "\n✗ Test 3 failed: " << result3.get_message() << std::endl;
    }

    std::cout << "\n✓ Multi-tool test completed" << std::endl;
}
void agent_call_agent(){
    auto registry = std::make_shared<ToolRegistry>();

    registry->register_tool<CalculatorTool>();
    registry->register_tool<GitHubTool>();
    registry->register_tool<Base64Tool>();

    auto cleanCodeAgent = registry->register_tool<Agent>(true);
    cleanCodeAgent->set_name("cleanCodeAgent");
    cleanCodeAgent->set_description("agent in charge of making sure everything is up to clean code standards");
    cleanCodeAgent->add_message("system", "Make sure everything is according to there rules Clean code principles from Uncle Bob's Clean Code emphasize writing code primarily for human readability and maintainability. Use meaningful, pronounceable names for variables and functions that clearly reveal their intent and purpose while avoiding cryptic abbreviations. Keep functions small (ideally 5-20 lines) with each function doing one thing only, minimizing arguments to 0-2 when possible, and using descriptive names that indicate their action. Apply the Single Responsibility Principle where each class or module has one reason to change, maintain clear separation of concerns, and structure code hierarchically with high-level abstractions at the top and details below. Prefer self-documenting code over comments, using comments only to explain why rather than what, and always remove commented-out or obsolete code. Handle errors using exceptions rather than return codes, avoid returning null by using Optional or empty objects instead, and keep error handling separate from business logic. Follow the Boy Scout Rule by leaving code cleaner than you found it, refactor continuously, and always remember that code is read ten times more than it is written, so optimize for readability above all else.");

    Agent agent(true);

    agent.set_tool_registry(registry);

    std::map<std::string, std::any> params = {
        {"message", std::string("Get the pull requests from the microsoft/vscode repository on GitHub. Find the most technical sounding one and look at its details. Make sure that all the files are following clean code principles by passing the actual code from the github to the clean code agent")},
        {"system_prompt", std::string("You are a helpful assistant with access to tools. Use them when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };


    auto result = agent.execute_safe(params);
}

void test_ollama_agent() {
    std::cout << "\n=== Test: Ollama Agent ===" << std::endl;

    try {
        // Create Ollama engine
        auto ollama_engine = std::make_shared<OllamaCompletionEngine>();

        // Create agent with Ollama engine
        Agent ollama_agent(ollama_engine, "llama3.1:8b", true);

        std::cout << "Testing Ollama agent with llama3.2 model\n" << std::endl;

        std::map<std::string, std::any> params = {
            {"message", std::string("Write a haiku about programming.")},
            {"system_prompt", std::string("You are a poetic assistant.")}
        };

        auto result = ollama_agent.execute_safe(params);

        if (result.is_success()) {
            std::cout << "\n✓ Ollama agent test passed" << std::endl;
        } else {
            std::cout << "\n✗ Ollama agent test failed: " << result.get_message() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠️  SKIPPED: " << e.what() << std::endl;
        std::cout << "Make sure Ollama is running: ollama serve" << std::endl;
        std::cout << "And llama3.2 is installed: ollama pull llama3.2" << std::endl;
    }
}

void test_ollama_to_chatgpt_conversation() {
    std::cout << "\n=== Test: Ollama Agent talking to ChatGPT Agent ===" << std::endl;

    // Check for API key
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    try {
        auto registry = std::make_shared<ToolRegistry>();

        registry->register_tool<CalculatorTool>();
        registry->register_tool<GitHubTool>();
        registry->register_tool<Base64Tool>();
        registry->register_tool<WebSearchTool>();
        registry->register_tool<WebFetchTool>();
            // Create Ollama agent
        auto ollama_engine = std::make_shared<OllamaCompletionEngine>();
        Agent ollama_agent(ollama_engine, "llama3.1:8b", true);
    
        ollama_agent.set_tool_registry(registry);


        // Create ChatGPT agent
        Agent chatgpt_agent(true);
        
        chatgpt_agent.set_tool_registry(registry);

        std::cout << "Ollama (llama3.2) will debate with ChatGPT (gpt-3.5-turbo) about tabs vs spaces\n" << std::endl;

        std::map<std::string, std::any> ollama_params = {
            {"name", std::string("Ollama Agent")},
            {"system_prompt", std::string("Youre fighting for equal rights for humans and robots. When you think you have won the debate, say COMPLETE, if you think there is any ambiguity to who won, dont complete. Provide sources for a better arguement using the websearch tool")}
        };

        std::map<std::string, std::any> chatgpt_params = {
            {"name", std::string("ChatGPT Agent")},
            {"system_prompt", std::string("You are fighting to keep the status quo at all costs. When you think you have won the debate, say COMPLETE, if you think there is any ambiguity to who won, dont complete. Provide sources for a better argument using the websearch tool")},
            {"model", std::string("gpt-4o-mini")}
        };

        ollama_agent.talk(chatgpt_agent, ollama_params, chatgpt_params);

        std::cout << "\n✓ Cross-engine conversation test passed" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "⚠️  Error: " << e.what() << std::endl;
        std::cout << "Make sure Ollama is running: ollama serve" << std::endl;
    }
}

void test_context_window_truncation() {
    std::cout << "\n=== Test: Context Window Truncation ===" << std::endl;

    Agent agent(true);
    if (!agent.has_api_key()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    std::cout << "Testing context window truncation with long conversation history\n" << std::endl;

    // Build a long conversation that will exceed gpt-3.5-turbo's 4096 token limit
    agent.add_message("system", "You are a helpful assistant that provides concise responses.");

    // Add many message pairs to exceed context window
    for (int i = 0; i < 50; i++) {
        std::string user_msg = "Question " + std::to_string(i) + ": Can you explain the importance of clean code practices in software development? Please provide detailed examples and best practices.";
        std::string assistant_msg = "Answer " + std::to_string(i) + ": Clean code is extremely important for maintainability, readability, and collaboration. Key principles include meaningful naming, small focused functions, proper error handling, and comprehensive testing. Examples include using descriptive variable names, keeping functions under 20 lines, applying SOLID principles, and writing self-documenting code.";

        agent.add_message("user", user_msg);
        agent.add_message("assistant", assistant_msg);
    }

    std::cout << "Created conversation with " << agent.get_history().size() << " messages" << std::endl;
    std::cout << "This should exceed gpt-3.5-turbo's 4096 token limit\n" << std::endl;

    // Now make a request - truncation should happen automatically
    std::map<std::string, std::any> params = {
        {"message", std::string("What is the most important principle of clean code?")},
        {"model", std::string("gpt-3.5-turbo")},
        {"max_tokens", 100}
    };

    std::cout << "Making request with model: gpt-3.5-turbo" << std::endl;
    std::cout << "Expected: Context window truncation warning, then successful response\n" << std::endl;

    auto result = agent.execute_safe(params);

    if (result.is_success()) {
        std::cout << "\n✓ Context window truncation test passed" << std::endl;
        std::cout << "  Response received despite long conversation history" << std::endl;
        std::cout << "  Final history size: " << agent.get_history().size() << " messages" << std::endl;
    } else {
        std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
    }
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║           ChatGPT Agent Verbose Mode Test Suite            ║
║                                                            ║
║  This test suite demonstrates the verbose parameter        ║
║  functionality for the Agent class.                 ║
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
        //test_verbose_constructor();
        //test_system_prompt_with_verbose();
        //test_multi_agent_system_prompt_with_verbose();
        agent_call_agent();

        // Ollama tests
        //test_ollama_agent();
        //test_ollama_to_chatgpt_conversation();

        // Context window truncation test
        //test_context_window_truncation();

        std::cout << "\nTest suite finished successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
