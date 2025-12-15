#include "../include/AvailableToolsRegistry.hpp"
#include "../include/Base64Tool.hpp"
#include "../include/WebSearchTool.hpp"
#include "../include/WebFetchTool.hpp"
#include "../include/GitHubTool.hpp"
#include "../include/CalculatorTool.hpp"

std::vector<AvailableToolsRegistry::ToolInfo> AvailableToolsRegistry::list_available() {
    return {
        {
            "base64",
            "Encode and decode Base64 strings",
            []() { return std::make_shared<Base64Tool>(); }
        },
        {
            "web_search",
            "Search the web using Brave Search API",
            []() { return std::make_shared<WebSearchTool>(); }
        },
        {
            "web_fetch",
            "Fetch and read web page content",
            []() { return std::make_shared<WebFetchTool>(); }
        },
        {
            "github",
            "Interact with GitHub repositories",
            []() { return std::make_shared<GitHubTool>(); }
        },
        {
            "calculator",
            "Perform basic arithmetic operations",
            []() { return std::make_shared<CalculatorTool>(); }
        }
    };
}

ToolPtr AvailableToolsRegistry::create_tool(const std::string& name) {
    for (const auto& info : list_available()) {
        if (info.name == name) {
            return info.factory();
        }
    }
    return nullptr;
}

bool AvailableToolsRegistry::is_available(const std::string& name) {
    for (const auto& info : list_available()) {
        if (info.name == name) {
            return true;
        }
    }
    return false;
}
