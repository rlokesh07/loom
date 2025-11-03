#include "../include/ToolRegistry.hpp"
#include <stdexcept>
#include <algorithm>

void ToolRegistry::register_tool(ToolPtr tool) {
    if (!tool) {
        throw std::invalid_argument("Cannot register null tool");
    }

    const std::string& name = tool->get_name();

    if (tools_.find(name) != tools_.end()) {
        throw std::runtime_error("Tool '" + name + "' is already registered");
    }

    tools_[name] = tool;
}

ToolPtr ToolRegistry::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

std::vector<std::string> ToolRegistry::list_tools() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());

    for (const auto& [name, tool] : tools_) {
        names.push_back(name);
    }

    return names;
}

json ToolRegistry::get_all_schemas() const {
    json schemas = json::array();

    for (const auto& [name, tool] : tools_) {
        json tool_schema = {
            {"type", "function"},
            {"function", tool->get_schema()}
        };
        schemas.push_back(tool_schema);
    }

    return schemas;
}

ToolResult ToolRegistry::execute_tool(const std::string& name, const json& params) {
    auto tool = get_tool(name);

    if (!tool) {
        return ToolResult::Error("Tool '" + name + "' not found in registry");
    }

    try {
        return tool->execute_from_json(params);
    } catch (const std::exception& e) {
        return ToolResult::Error("Error executing tool '" + name + "': " + e.what());
    }
}

bool ToolRegistry::unregister_tool(const std::string& name) {
    return tools_.erase(name) > 0;
}

void ToolRegistry::clear() {
    tools_.clear();
}
