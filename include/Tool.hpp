#ifndef TOOL_HPP
#define TOOL_HPP

#include "Parameter.hpp"
#include "ToolResult.hpp"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Abstract base class for all tools
 *
 * Tools are executable components that can be invoked by agents with
 * validated parameters. Each tool defines its schema, parameters, and
 * execution logic.
 */
class Tool {
protected:
    std::string name_;
    std::string description_;
    std::vector<Parameter> parameters_;
    std::map<std::string, std::any> state_;  // Tool-specific persistent state

    /**
     * @brief Validate provided parameters against tool schema
     *
     * @param params Parameters to validate
     * @return true if all required parameters are present and valid
     */
    virtual bool validate_parameters(const std::map<std::string, std::any>& params) const;

public:
    /**
     * @brief Construct a new Tool
     *
     * @param name Unique identifier for the tool
     * @param description Human-readable description of tool functionality
     */
    Tool(const std::string& name, const std::string& description);

    virtual ~Tool() = default;

    // Prevent copying
    Tool(const Tool&) = delete;
    Tool& operator=(const Tool&) = delete;

    // Allow moving
    Tool(Tool&&) = default;
    Tool& operator=(Tool&&) = default;

    /**
     * @brief Execute the tool with provided parameters
     *
     * Must be implemented by derived classes to define tool behavior.
     *
     * @param params Parameters for execution
     * @return ToolResult containing execution outcome
     */
    virtual ToolResult execute(const std::map<std::string, std::any>& params) = 0;

    // Getters
    const std::string& get_name() const { return name_; }
    const std::string& get_description() const { return description_; }
    const std::vector<Parameter>& get_parameters() const { return parameters_; }

    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_description(const std::string& description) { description_ = description; }

    /**
     * @brief Add a parameter definition to this tool
     */
    void add_parameter(const Parameter& param);

    /**
     * @brief Add multiple parameters at once
     */
    void add_parameters(const std::vector<Parameter>& params);

    /**
     * @brief Get tool schema in JSON format for LLM consumption
     *
     * @return json Schema following OpenAI function calling format
     */
    virtual json get_schema() const;

    /**
     * @brief Execute with automatic parameter validation
     *
     * Wraps execute() with validation and error handling.
     *
     * @param params Parameters for execution
     * @return ToolResult containing execution outcome or validation error
     */
    ToolResult execute_safe(const std::map<std::string, std::any>& params);

    /**
     * @brief Execute with JSON parameters (converts to std::any map)
     */
    ToolResult execute_from_json(const json& params);

    // ========== State Management ==========

    /**
     * @brief Set a state value for this tool instance
     */
    void set_state(const std::string& key, const std::any& value);

    /**
     * @brief Get a state value
     */
    std::optional<std::any> get_state(const std::string& key) const;

    /**
     * @brief Check if state key exists
     */
    bool has_state(const std::string& key) const;
    /**
     * @brief Clear all state
     */
    void clear_state();
    
    /**
     * @brief Template method to set typed state
     */
    template<typename T>
    void set_state_typed(const std::string& key, const T& value) {
        state_[key] = value;
    }

    /**
     * @brief Template method to get typed state
     */
    template<typename T>
    std::optional<T> get_state_typed(const std::string& key) const {
        auto it = state_.find(key);
        if (it != state_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief Shared pointer type for tools
 */
using ToolPtr = std::shared_ptr<Tool>;

#endif // TOOL_HPP
