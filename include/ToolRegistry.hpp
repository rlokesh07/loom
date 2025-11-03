#ifndef TOOL_REGISTRY_HPP
#define TOOL_REGISTRY_HPP

#include "Tool.hpp"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Central registry for managing tools available to agents
 *
 * The ToolRegistry acts as a tool manager that:
 * - Registers and stores tools by name
 * - Generates combined JSON schemas for LLM function calling
 * - Executes tools by name with JSON parameters
 * - Provides tool discovery and introspection
 */
class ToolRegistry {
private:
    std::map<std::string, ToolPtr> tools_;

public:
    ToolRegistry() = default;

    /**
     * @brief Register a tool in the registry
     *
     * @param tool Shared pointer to the tool to register
     * @throws std::runtime_error if a tool with the same name already exists
     */
    void register_tool(ToolPtr tool);

    /**
     * @brief Get a tool by name
     *
     * @param name Name of the tool to retrieve
     * @return ToolPtr Shared pointer to the tool, or nullptr if not found
     */
    ToolPtr get_tool(const std::string& name) const;

    /**
     * @brief Check if a tool with the given name exists
     *
     * @param name Name of the tool to check
     * @return true if the tool exists, false otherwise
     */
    bool has_tool(const std::string& name) const;

    /**
     * @brief Get all tool names
     *
     * @return std::vector<std::string> List of all registered tool names
     */
    std::vector<std::string> list_tools() const;

    /**
     * @brief Get combined JSON schema for all tools
     *
     * Returns an array of tool schemas in OpenAI function calling format.
     * This can be directly used in the "tools" parameter of chat completions.
     *
     * @return json Array of tool schemas
     */
    json get_all_schemas() const;

    /**
     * @brief Execute a tool by name with JSON parameters
     *
     * @param name Name of the tool to execute
     * @param params JSON object containing parameters
     * @return ToolResult Result of the tool execution
     */
    ToolResult execute_tool(const std::string& name, const json& params);

    /**
     * @brief Get the number of registered tools
     *
     * @return size_t Number of tools in the registry
     */
    size_t size() const { return tools_.size(); }

    /**
     * @brief Remove a tool from the registry
     *
     * @param name Name of the tool to remove
     * @return true if the tool was removed, false if it didn't exist
     */
    bool unregister_tool(const std::string& name);

    /**
     * @brief Clear all registered tools
     */
    void clear();
};

/**
 * @brief Shared pointer type for ToolRegistry
 */
using ToolRegistryPtr = std::shared_ptr<ToolRegistry>;

#endif // TOOL_REGISTRY_HPP
