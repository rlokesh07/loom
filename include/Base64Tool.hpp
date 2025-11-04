#ifndef BASE64_TOOL_HPP
#define BASE64_TOOL_HPP

#include "Tool.hpp"
#include <string>
#include <vector>

/**
 * @brief Tool for Base64 encoding and decoding operations
 */
class Base64Tool : public Tool {
public:
    Base64Tool();

    /**
     * @brief Execute Base64 operation
     *
     * Parameters:
     * - operation (string): "encode" or "decode"
     * - input (string): Text to encode or Base64 string to decode
     */
    ToolResult execute(const std::map<std::string, std::any>& params) override;

private:
    std::string get_string_param(const std::any& value) const;

    // Base64 encoding/decoding helper functions
    std::string base64_encode(const std::string& input) const;
    std::string base64_decode(const std::string& input) const;

    static constexpr char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    bool is_base64(unsigned char c) const;
};

#endif // BASE64_TOOL_HPP
