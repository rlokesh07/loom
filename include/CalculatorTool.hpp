#ifndef CALCULATOR_TOOL_HPP
#define CALCULATOR_TOOL_HPP

#include "Tool.hpp"

/**
 * @brief Example tool that performs basic arithmetic operations
 */
class CalculatorTool : public Tool {
public:
    CalculatorTool();

    /**
     * @brief Execute arithmetic operation
     *
     * Parameters:
     * - operation (string): "add", "subtract", "multiply", or "divide"
     * - a (number): First operand
     * - b (number): Second operand
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

private:
    double get_number_param(const std::any& value) const;
    std::string get_string_param(const std::any& value) const;
};

#endif // CALCULATOR_TOOL_HPP
