#include "../include/Tool.hpp"
#include <stdexcept>

Tool::Tool(const std::string& name, const std::string& description)
    : name_(name),
      description_(description),
      parameters_(),
      state_() {}

bool Tool::validate_parameters(const std::map<std::string, std::any>& params) const {
    // Check all required parameters are present
    for (const auto& param : parameters_) {
        if (param.is_required() && params.find(param.get_name()) == params.end()) {
            return false;
        }

        // If parameter is provided, validate it
        auto it = params.find(param.get_name());
        if (it != params.end()) {
            if (!param.validate(it->second)) {
                return false;
            }
        }
    }

    return true;
}

void Tool::add_parameter(const Parameter& param) {
    parameters_.push_back(param);
}

void Tool::add_parameters(const std::vector<Parameter>& params) {
    parameters_.insert(parameters_.end(), params.begin(), params.end());
}

json Tool::get_schema() const {
    json schema;
    schema["name"] = name_;
    schema["description"] = description_;

    json params_json = json::object();
    json required_params = json::array();

    for (const auto& param : parameters_) {
        json param_json;
        param_json["type"] = Parameter::type_to_string(param.get_type());
        param_json["description"] = param.get_description();

        params_json[param.get_name()] = param_json;

        if (param.is_required()) {
            required_params.push_back(param.get_name());
        }
    }

    schema["parameters"] = {
        {"type", "object"},
        {"properties", params_json},
        {"required", required_params}
    };

    return schema;
}

ToolResult Tool::execute_safe(const std::map<std::string, std::any>& params) {
    try {
        if (!validate_parameters(params)) {
            return ToolResult::Error("Invalid parameters provided");
        }
        return execute(params);
    } catch (const std::exception& e) {
        return ToolResult::Error(std::string("Execution error: ") + e.what());
    } catch (...) {
        return ToolResult::Error("Unknown execution error occurred");
    }
}

ToolResult Tool::execute_from_json(const json& params) {
    std::map<std::string, std::any> params_map;

    // Convert JSON to std::any map
    for (auto it = params.begin(); it != params.end(); ++it) {
        const auto& value = it.value();

        if (value.is_string()) {
            params_map[it.key()] = value.get<std::string>();
        } else if (value.is_number_integer()) {
            params_map[it.key()] = value.get<int>();
        } else if (value.is_number_float()) {
            params_map[it.key()] = value.get<double>();
        } else if (value.is_boolean()) {
            params_map[it.key()] = value.get<bool>();
        } else if (value.is_array() || value.is_object()) {
            params_map[it.key()] = value;  // Store as json
        } else {
            params_map[it.key()] = std::any();  // null or unknown
        }
    }

    return execute_safe(params_map);
}

void Tool::set_state(const std::string& key, const std::any& value) {
    state_[key] = value;
}

std::optional<std::any> Tool::get_state(const std::string& key) const {
    auto it = state_.find(key);
    if (it != state_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Tool::has_state(const std::string& key) const {
    return state_.find(key) != state_.end();
}

void Tool::clear_state() {
    state_.clear();
}
