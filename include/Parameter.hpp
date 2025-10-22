#ifndef PARAMETER_HPP
#define PARAMETER_HPP

#include <string>
#include <any>
#include <optional>
#include <functional>

/**
 * @brief Enumeration of supported parameter types
 */
enum class ParameterType {
    STRING,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ARRAY,
    OBJECT,
    ANY
};

/**
 * @brief Represents a parameter definition for a tool
 */
class Parameter {
private:
    std::string name_;
    ParameterType type_;
    std::string description_;
    bool required_;
    std::any default_value_;
    std::optional<std::function<bool(const std::any&)>> validator_;

public:
    /**
     * @brief Construct a new Parameter
     *
     * @param name Parameter name
     * @param type Parameter type
     * @param description Human-readable description
     * @param required Whether this parameter is required
     */
    Parameter(const std::string& name,
              ParameterType type,
              const std::string& description,
              bool required = true);

    // Getters
    const std::string& get_name() const { return name_; }
    ParameterType get_type() const { return type_; }
    const std::string& get_description() const { return description_; }
    bool is_required() const { return required_; }
    const std::any& get_default_value() const { return default_value_; }

    /**
     * @brief Check if parameter has a default value
     */
    bool has_default_value() const { return default_value_.has_value(); }

    /**
     * @brief Set default value for optional parameters
     */
    void set_default_value(const std::any& value);

    /**
     * @brief Set custom validator function
     */
    void set_validator(const std::function<bool(const std::any&)>& validator);

    /**
     * @brief Validate a value against this parameter's constraints
     */
    bool validate(const std::any& value) const;

    /**
     * @brief Convert parameter type to JSON schema string
     */
    static std::string type_to_string(ParameterType type);
};

#endif // PARAMETER_HPP
