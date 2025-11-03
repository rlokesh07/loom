#include "include/GitHubTool.hpp" 
#include "include/ToolRegistry.hpp"
#include <iostream>

int main() {
    auto github = std::make_shared<GitHubTool>();
    auto registry = std::make_shared<ToolRegistry>();
    registry->register_tool(github);
    
    std::cout << "GitHub Tool Schema:" << std::endl;
    std::cout << github->get_schema().dump(2) << std::endl;
    
    std::cout << "\nAll Schemas from Registry:" << std::endl;
    std::cout << registry->get_all_schemas().dump(2) << std::endl;
    
    return 0;
}