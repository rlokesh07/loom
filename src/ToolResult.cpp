#include "ToolResult.hpp"

ToolResult::ToolResult(bool success, const std::any& data, const std::string& message)
    : success_(success),
      data_(data),
      message_(message),
      error_(std::nullopt) {}

void ToolResult::set_error(const std::string& error) {
    success_ = false;
    error_ = error;
}

ToolResult ToolResult::Success(const std::any& data, const std::string& message) {
    return ToolResult(true, data, message);
}

ToolResult ToolResult::Error(const std::string& error_message) {
    ToolResult result(false, std::any(), "");
    result.set_error(error_message);
    return result;
}

json ToolResult::to_json() const {
    json j;
    j["success"] = success_;
    j["message"] = message_;

    if (error_) {
        j["error"] = *error_;
    }

    // Basic data serialization
    // For complex types, derived classes should override this method
    if (data_.has_value()) {
        // Try to convert common types
        try {
            if (data_.type() == typeid(std::string)) {
                j["data"] = std::any_cast<std::string>(data_);
            } else if (data_.type() == typeid(int)) {
                j["data"] = std::any_cast<int>(data_);
            } else if (data_.type() == typeid(double)) {
                j["data"] = std::any_cast<double>(data_);
            } else if (data_.type() == typeid(bool)) {
                j["data"] = std::any_cast<bool>(data_);
            } else if (data_.type() == typeid(json)) {
                j["data"] = std::any_cast<json>(data_);
            } else {
                j["data"] = "<<non-serializable data>>";
            }
        } catch (...) {
            j["data"] = "<<serialization error>>";
        }
    } else {
        j["data"] = nullptr;
    }

    return j;
}
