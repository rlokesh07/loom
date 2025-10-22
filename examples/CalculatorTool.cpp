#include "./CalculatorTool.hpp"
#include <cmath>

CalculatorTool::CalculatorTool()
    : Tool("calculator", "Performs basic arithmetic operations") {

    // Define parameters
    add_parameter(Parameter("operation", ParameterType::STRING,
                           "The operation to perform: add, subtract, multiply, or divide",
                           true));

    add_parameter(Parameter("a", ParameterType::FLOAT,
                           "First operand", true));

    add_parameter(Parameter("b", ParameterType::FLOAT,
                           "Second operand", true));
}

ToolResult CalculatorTool::execute(const std::map<std::string, std::any>& params) {
    try {
        // Extract parameters
        std::string operation = get_string_param(params.at("operation"));
        double a = get_number_param(params.at("a"));
        double b = get_number_param(params.at("b"));

        double result = 0.0;

        // Perform operation
        if (operation == "add") {
            result = a + b;
        } else if (operation == "subtract") {
            result = a - b;
        } else if (operation == "multiply") {
            result = a * b;
        } else if (operation == "divide") {
            if (std::abs(b) < 1e-10) {
                return ToolResult::Error("Division by zero");
            }
            result = a / b;
        } else {
            return ToolResult::Error("Unknown operation: " + operation);
        }

        // Return success with result
        return ToolResult::Success(result,
                                  "Calculated: " + std::to_string(a) + " " +
                                  operation + " " + std::to_string(b) +
                                  " = " + std::to_string(result));

    } catch (const std::out_of_range& e) {
        return ToolResult::Error("Missing required parameter");
    } catch (const std::exception& e) {
        return ToolResult::Error(std::string("Error: ") + e.what());
    }
}

double CalculatorTool::get_number_param(const std::any& value) const {
    // Try different numeric types
    if (value.type() == typeid(double)) {
        return std::any_cast<double>(value);
    } else if (value.type() == typeid(int)) {
        return static_cast<double>(std::any_cast<int>(value));
    } else if (value.type() == typeid(float)) {
        return static_cast<double>(std::any_cast<float>(value));
    }
    throw std::bad_any_cast();
}

std::string CalculatorTool::get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}
