#ifndef AVAILABLE_TOOLS_REGISTRY_HPP
#define AVAILABLE_TOOLS_REGISTRY_HPP

#include "Tool.hpp"
#include <vector>
#include <string>
#include <functional>

/**
 * @brief Static registry of all tools available for registration
 *
 * Provides a central catalog of tools that can be registered with agent sessions.
 * Each tool has a factory function to create new instances on demand.
 */
class AvailableToolsRegistry {
public:
    /**
     * @brief Information about an available tool
     */
    struct ToolInfo {
        std::string name;
        std::string description;
        std::function<ToolPtr()> factory;
    };

    /**
     * @brief Get list of all available tools
     *
     * @return std::vector<ToolInfo> List of tool information with factories
     */
    static std::vector<ToolInfo> list_available();

    /**
     * @brief Create a tool instance by name
     *
     * @param name Name of the tool to create
     * @return ToolPtr New tool instance, or nullptr if not found
     */
    static ToolPtr create_tool(const std::string& name);

    /**
     * @brief Check if a tool name is available
     *
     * @param name Name of the tool to check
     * @return true if the tool is available, false otherwise
     */
    static bool is_available(const std::string& name);
};

#endif // AVAILABLE_TOOLS_REGISTRY_HPP
