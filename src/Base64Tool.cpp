#include "../include/Base64Tool.hpp"
#include <stdexcept>

Base64Tool::Base64Tool()
    : Tool("base64", "Encodes and decodes Base64 strings") {

    // Define parameters
    add_parameter(Parameter("operation", ParameterType::STRING,
                           "The operation to perform: encode or decode",
                           true));

    add_parameter(Parameter("input", ParameterType::STRING,
                           "Text to encode or Base64 string to decode", true));
}

ToolResult Base64Tool::execute(const std::map<std::string, std::any>& params) {
    try {
        // Extract parameters
        std::string operation = get_string_param(params.at("operation"));
        std::string input = get_string_param(params.at("input"));

        std::string result;

        // Perform operation
        if (operation == "encode") {
            result = base64_encode(input);
            return ToolResult::Success(result, "Encoded: " + result);
        } else if (operation == "decode") {
            try {
                result = base64_decode(input);
                return ToolResult::Success(result, "Decoded: " + result);
            } catch (const std::exception& e) {
                return ToolResult::Error("Invalid Base64 input: " + std::string(e.what()));
            }
        } else {
            return ToolResult::Error("Unknown operation: " + operation + ". Use 'encode' or 'decode'");
        }

    } catch (const std::out_of_range& e) {
        return ToolResult::Error("Missing required parameter");
    } catch (const std::exception& e) {
        return ToolResult::Error(std::string("Error: ") + e.what());
    }
}

std::string Base64Tool::get_string_param(const std::any& value) const {
    return std::any_cast<std::string>(value);
}

std::string Base64Tool::base64_encode(const std::string& input) const {
    std::string encoded;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.length();
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                encoded += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while((i++ < 3))
            encoded += '=';
    }

    return encoded;
}

std::string Base64Tool::base64_decode(const std::string& encoded_string) const {
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string decoded;

    while (in_len-- && encoded_string[in_] != '=') {
        if (!is_base64(static_cast<unsigned char>(encoded_string[in_]))) {
            throw std::invalid_argument("Invalid Base64 character encountered");
        }

        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                // Find position in base64_chars
                char_array_4[i] = static_cast<unsigned char>(
                    std::string(base64_chars).find(char_array_4[i])
                );
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                decoded += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = static_cast<unsigned char>(
                std::string(base64_chars).find(char_array_4[j])
            );

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; j < i - 1; j++)
            decoded += char_array_3[j];
    }

    return decoded;
}

bool Base64Tool::is_base64(unsigned char c) const {
    return (isalnum(c) || (c == '+') || (c == '/'));
}
