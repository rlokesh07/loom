#include "../include/Agent.hpp"
#include "../include/OllamaCompletionEngine.hpp"
#include "../include/ToolRegistry.hpp"
#include "../include/WebSearchTool.hpp"
#include "../include/WebFetchTool.hpp"
#include "../include/Base64Tool.hpp"
#include <iostream>
#include <memory>

/**
 * WebSearch & WebFetch Tool Example
 *
 * Demonstrates:
 * 1. Direct WebSearchTool usage
 * 2. Integration with Agent for autonomous web search
 * 3. Integration with Ollama for local LLM web search
 * 4. Combined WebSearch + WebFetch for full research workflow
 *
 * Prerequisites:
 * - BRAVE_SEARCH_API_KEY environment variable set
 * - For ChatGPT: OPENAI_API_KEY environment variable set
 * - For Ollama: Ollama running locally with llama3.1:8b model
 */

void test_direct_websearch() {
    std::cout << "\n=== Test 1: Direct WebSearch Tool Usage ===" << std::endl;

    WebSearchTool search_tool;

    if (!search_tool.has_api_key()) {
        std::cout << "⚠️  SKIPPED: BRAVE_SEARCH_API_KEY not set" << std::endl;
        std::cout << "Get your API key from: https://api.search.brave.com/app/keys" << std::endl;
        return;
    }

    std::cout << "✓ BRAVE_SEARCH_API_KEY is set\n" << std::endl;

    // Test search
    std::map<std::string, std::any> params = {
        {"query", std::string("latest advances in artificial intelligence 2024")},
        {"count", 5}
    };

    std::cout << "Searching for: 'latest advances in artificial intelligence 2024'" << std::endl;
    std::cout << "Requesting 5 results...\n" << std::endl;

    auto result = search_tool.execute(params);

    if (result.is_success()) {
        std::cout << result.get_message() << std::endl;
        std::cout << "\n✓ Direct search test passed" << std::endl;
    } else {
        std::cout << "✗ Search failed: " << result.get_message() << std::endl;
    }
}

void test_chatgpt_with_websearch() {
    std::cout << "\n=== Test 2: ChatGPT Agent with WebSearch ===" << std::endl;

    // Check for required API keys
    const char* openai_key = std::getenv("OPENAI_API_KEY");
    if (!openai_key || std::string(openai_key).empty()) {
        std::cout << "⚠️  SKIPPED: OPENAI_API_KEY not set" << std::endl;
        return;
    }

    const char* brave_key = std::getenv("BRAVE_SEARCH_API_KEY");
    if (!brave_key || std::string(brave_key).empty()) {
        std::cout << "⚠️  SKIPPED: BRAVE_SEARCH_API_KEY not set" << std::endl;
        return;
    }

    std::cout << "✓ Both API keys are set\n" << std::endl;

    // Create agent and registry
    Agent agent(true);  // Verbose mode
    auto registry = std::make_shared<ToolRegistry>();

    // Register WebSearch tool
    registry->register_tool<WebSearchTool>();

    std::cout << "✓ Registered WebSearch tool" << std::endl;

    agent.set_tool_registry(registry);

    // Test: Agent should autonomously use web search
    std::map<std::string, std::any> params = {
        {"message", std::string("What are the latest breakthroughs in quantum computing? Search the web and give me a summary of the top 3 most recent developments.")},
        {"system_prompt", std::string("You are a helpful research assistant with access to web search. Use the search tool to find current information when needed.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "\nUser: What are the latest breakthroughs in quantum computing?" << std::endl;
    std::cout << "Expected: Agent will use web_search tool and summarize results\n" << std::endl;

    auto result = agent.execute_safe(params);

    if (result.is_success()) {
        std::cout << "\n✓ ChatGPT + WebSearch test passed" << std::endl;
    } else {
        std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
    }
}

void test_ollama_with_websearch() {
    std::cout << "\n=== Test 3: Ollama Agent with WebSearch ===" << std::endl;

    // Check for Brave API key
    const char* brave_key = std::getenv("BRAVE_SEARCH_API_KEY");
    if (!brave_key || std::string(brave_key).empty()) {
        std::cout << "⚠️  SKIPPED: BRAVE_SEARCH_API_KEY not set" << std::endl;
        return;
    }

    try {
        // Create Ollama engine and agent
        auto ollama_engine = std::make_shared<OllamaCompletionEngine>();
        Agent ollama_agent(ollama_engine, "llama3.1:8b", true);

        std::cout << "✓ Created Ollama agent with llama3.1:8b model" << std::endl;

        // Create registry and register tools
        auto registry = std::make_shared<ToolRegistry>();
        registry->register_tool<WebSearchTool>();

        std::cout << "✓ Registered WebSearch tool" << std::endl;

        ollama_agent.set_tool_registry(registry);

        // Test local LLM with web search
        std::map<std::string, std::any> params = {
            {"message", std::string("Search the web for the current Bitcoin price and tell me what you found.")},
            {"system_prompt", std::string("You are a helpful assistant with access to web search. Use it when you need current information.")}
        };

        std::cout << "\nUser: Search the web for the current Bitcoin price" << std::endl;
        std::cout << "Expected: Ollama agent will use web_search tool\n" << std::endl;

        auto result = ollama_agent.execute_safe(params);

        if (result.is_success()) {
            std::cout << "\n✓ Ollama + WebSearch test passed" << std::endl;
        } else {
            std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠️  SKIPPED: " << e.what() << std::endl;
        std::cout << "Make sure Ollama is running: ollama serve" << std::endl;
        std::cout << "And llama3.1:8b is installed: ollama pull llama3.1:8b" << std::endl;
    }
}

void test_multi_tool_research() {
    std::cout << "\n=== Test 4: Multi-Tool Research (WebSearch + WebFetch) ===" << std::endl;

    const char* openai_key = std::getenv("OPENAI_API_KEY");
    const char* brave_key = std::getenv("BRAVE_SEARCH_API_KEY");

    if (!openai_key || !brave_key) {
        std::cout << "⚠️  SKIPPED: Missing required API keys" << std::endl;
        return;
    }

    Agent agent(true);
    auto registry = std::make_shared<ToolRegistry>();

    // Register web research tools
    registry->register_tool<WebSearchTool>();
    registry->register_tool<WebFetchTool>();

    std::cout << "✓ Registered " << registry->size() << " tools: ";
    for (const auto& tool_name : registry->list_tools()) {
        std::cout << tool_name << " ";
    }
    std::cout << "\n" << std::endl;

    agent.set_tool_registry(registry);

    std::map<std::string, std::any> params = {
        {"message", std::string("Search for 'C++ best practices' on the web, then fetch and read the content from the top result to give me a summary of the key points.")},
        {"system_prompt", std::string("You are a research assistant. First use web_search to find relevant pages, then use web_fetch to read their content and provide summaries.")},
        {"model", std::string("gpt-4o-mini")}
    };

    std::cout << "User: Search for C++ best practices and read the top result" << std::endl;
    std::cout << "Expected: Agent will search the web, then fetch and read the top URL\n" << std::endl;

    auto result = agent.execute_safe(params);

    if (result.is_success()) {
        std::cout << "\n✓ Multi-tool research test passed" << std::endl;
    } else {
        std::cout << "\n✗ Test failed: " << result.get_message() << std::endl;
    }
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║         WebSearch & WebFetch Tool Example Suite           ║
║                                                            ║
║  Demonstrates web research capabilities:                  ║
║  - WebSearch: Search the web using Brave Search API       ║
║  - WebFetch: Fetch and read content from URLs             ║
║  - Combined workflow: Search → Fetch → Analyze            ║
║                                                            ║
║  Prerequisites:                                            ║
║  - BRAVE_SEARCH_API_KEY (required for search tests)       ║
║    Get yours at: https://api.search.brave.com/app/keys    ║
║  - OPENAI_API_KEY (optional, for ChatGPT tests)           ║
║  - Ollama running locally (optional, for Ollama tests)    ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    try {
        // Run all tests
        test_direct_websearch();
        test_chatgpt_with_websearch();
        test_ollama_with_websearch();
        test_multi_tool_research();

        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  Test suite completed!                                     ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
