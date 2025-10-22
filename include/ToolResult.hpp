#ifndef TOOL_RESULT_HPP
#define TOOL_RESULT_HPP

#include <string>
#include <any>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Represents the result of a tool execution
 */
class ToolResult {
private:
    bool success_;
    std::any data_;
    std::string message_;
    std::optional<std::string> error_;

public:
    /**
     * @brief Construct a new Tool Result
     *
     * @param success Whether execution was successful
     * @param data Result data (type-erased)
     * @param message Optional status message
     */
    ToolResult(bool success, const std::any& data, const std::string& message = "");

    // Getters
    bool is_success() const { return success_; }
    const std::any& get_data() const { return data_; }
    const std::string& get_message() const { return message_; }
    const std::optional<std::string>& get_error() const { return error_; }

    /**
     * @brief Set error message (marks result as failed)
     */
    void set_error(const std::string& error);

    /**
     * @brief Create a successful result
     */
    static ToolResult Success(const std::any& data, const std::string& message = "");

    /**
     * @brief Create an error result
     */
    static ToolResult Error(const std::string& error_message);

    /**
     * @brief Convert result to JSON for agent consumption
     */
    json to_json() const;

    /**
     * @brief Template method to safely extract typed data
     * @throws std::bad_any_cast if type doesn't match
     */
    template<typename T>
    T get_data_as() const {
        return std::any_cast<T>(data_);
    }

    /**
     * @brief Try to extract typed data, returns optional
     */
    template<typename T>
    std::optional<T> try_get_data_as() const {
        try {
            return std::any_cast<T>(data_);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
};

#endif // TOOL_RESULT_HPP
