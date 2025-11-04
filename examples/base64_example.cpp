#include "../include/Base64Tool.hpp"
#include <iostream>
#include <map>
#include <any>

int main() {
    Base64Tool base64;

    std::cout << "=== Base64 Tool Example ===" << std::endl << std::endl;

    // Test 1: Encode a string
    std::cout << "Test 1: Encoding a string" << std::endl;
    std::map<std::string, std::any> encode_params = {
        {"operation", std::string("encode")},
        {"input", std::string("Hello, World!")}
    };

    auto encode_result = base64.execute(encode_params);
    if (encode_result.is_success()) {
        std::cout << "✓ Success: " << encode_result.get_message() << std::endl;
        std::cout << "  Encoded value: " << std::any_cast<std::string>(encode_result.get_data()) << std::endl;
    } else {
        auto error = encode_result.get_error();
        std::cout << "✗ Error: " << (error.has_value() ? *error : "Unknown error") << std::endl;
    }

    std::cout << std::endl;

    // Test 2: Decode a Base64 string
    std::cout << "Test 2: Decoding a Base64 string" << std::endl;
    std::map<std::string, std::any> decode_params = {
        {"operation", std::string("decode")},
        {"input", std::string("SGVsbG8sIFdvcmxkIQ==")}
    };

    auto decode_result = base64.execute(decode_params);
    if (decode_result.is_success()) {
        std::cout << "✓ Success: " << decode_result.get_message() << std::endl;
        std::cout << "  Decoded value: " << std::any_cast<std::string>(decode_result.get_data()) << std::endl;
    } else {
        auto error = decode_result.get_error();
        std::cout << "✗ Error: " << (error.has_value() ? *error : "Unknown error") << std::endl;
    }

    std::cout << std::endl;

    // Test 3: Invalid operation
    std::cout << "Test 3: Invalid operation" << std::endl;
    std::map<std::string, std::any> invalid_params = {
        {"operation", std::string("invalid")},
        {"input", std::string("test")}
    };

    auto invalid_result = base64.execute(invalid_params);
    if (!invalid_result.is_success()) {
        auto error = invalid_result.get_error();
        std::cout << "✓ Expected error: " << (error.has_value() ? *error : "Unknown error") << std::endl;
    }

    std::cout << std::endl;

    // Test 4: Invalid Base64 input
    std::cout << "Test 4: Invalid Base64 input" << std::endl;
    std::map<std::string, std::any> invalid_base64_params = {
        {"operation", std::string("decode")},
        {"input", std::string("This is not valid Base64!!!")}
    };

    auto invalid_base64_result = base64.execute(invalid_base64_params);
    if (!invalid_base64_result.is_success()) {
        auto error = invalid_base64_result.get_error();
        std::cout << "✓ Expected error: " << (error.has_value() ? *error : "Unknown error") << std::endl;
    }

    std::cout << std::endl;

    // Test 5: Get JSON schema
    std::cout << "Test 5: JSON Schema for LLM" << std::endl;
    json schema = base64.get_schema();
    std::cout << schema.dump(2) << std::endl;

    return 0;
}
