#include "CalculatorTool.hpp"
#include <iostream>
#include <iomanip>

void print_result(const ToolResult& result) {
    std::cout << "Success: " << (result.is_success() ? "true" : "false") << "\n";
    std::cout << "Message: " << result.get_message() << "\n";

    if (result.get_error()) {
        std::cout << "Error: " << *result.get_error() << "\n";
    }

    if (result.is_success()) {
        auto value = result.try_get_data_as<double>();
        if (value) {
            std::cout << "Result: " << *value << "\n";
        }
    }

    std::cout << "\nJSON representation:\n";
    std::cout << result.to_json().dump(2) << "\n";
    std::cout << std::string(50, '-') << "\n\n";
}

int main() {
    std::cout << "=== Tool System Example ===\n\n";

    // Create calculator tool
    CalculatorTool calc;

    // Print tool schema
    std::cout << "Tool Schema:\n";
    std::cout << calc.get_schema().dump(2) << "\n\n";
    std::cout << std::string(50, '=') << "\n\n";

    // Example 1: Addition
    std::cout << "Example 1: Addition\n";
    std::map<std::string, std::any> params1 = {
        {"operation", std::string("add")},
        {"a", 10.5},
        {"b", 5.3}
    };
    auto result1 = calc.execute_safe(params1);
    print_result(result1);

    // Example 2: Multiplication
    std::cout << "Example 2: Multiplication\n";
    std::map<std::string, std::any> params2 = {
        {"operation", std::string("multiply")},
        {"a", 7.0},
        {"b", 6.0}
    };
    auto result2 = calc.execute_safe(params2);
    print_result(result2);

    // Example 3: Division
    std::cout << "Example 3: Division\n";
    std::map<std::string, std::any> params3 = {
        {"operation", std::string("divide")},
        {"a", 100.0},
        {"b", 4.0}
    };
    auto result3 = calc.execute_safe(params3);
    print_result(result3);

    // Example 4: Error case - division by zero
    std::cout << "Example 4: Division by zero (error case)\n";
    std::map<std::string, std::any> params4 = {
        {"operation", std::string("divide")},
        {"a", 10.0},
        {"b", 0.0}
    };
    auto result4 = calc.execute_safe(params4);
    print_result(result4);

    // Example 5: Error case - missing parameter
    std::cout << "Example 5: Missing parameter (error case)\n";
    std::map<std::string, std::any> params5 = {
        {"operation", std::string("add")},
        {"a", 10.0}
        // Missing 'b' parameter
    };
    auto result5 = calc.execute_safe(params5);
    print_result(result5);

    // Example 6: Using JSON parameters
    std::cout << "Example 6: Subtraction (from JSON)\n";
    json json_params = {
        {"operation", "subtract"},
        {"a", 50},
        {"b", 12}
    };
    auto result6 = calc.execute_from_json(json_params);
    print_result(result6);

    return 0;
}
