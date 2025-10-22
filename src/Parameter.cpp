#include "Parameter.hpp"
#include <stdexcept>

Parameter::Parameter(const std::string& name,
                     ParameterType type,
                     const std::string& description,
                     bool required)
    : name_(name),
      type_(type),
      description_(description),
      required_(required),
      default_value_(),
      validator_(std::nullopt) {}

void Parameter::set_default_value(const std::any& value) {
    default_value_ = value;
}

void Parameter::set_validator(const std::function<bool(const std::any&)>& validator) {
    validator_ = validator;
}

bool Parameter::validate(const std::any& value) const {
    // If no validator is set, consider it valid
    if (!validator_) {
        return true;
    }

    // Run custom validator
    return (*validator_)(value);
}

std::string Parameter::type_to_string(ParameterType type) {
    switch (type) {
        case ParameterType::STRING:
            return "string";
        case ParameterType::INTEGER:
            return "integer";
        case ParameterType::FLOAT:
            return "number";
        case ParameterType::BOOLEAN:
            return "boolean";
        case ParameterType::ARRAY:
            return "array";
        case ParameterType::OBJECT:
            return "object";
        case ParameterType::ANY:
            return "any";
        default:
            throw std::invalid_argument("Unknown parameter type");
    }
}
