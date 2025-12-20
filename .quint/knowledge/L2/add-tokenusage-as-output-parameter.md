---
scope: Works across all CompletionEngine implementations. Ideal for gradual migration where some callers need usage and others don't. Compatible with C++11 and higher.
kind: system
---

# Hypothesis: Add TokenUsage as output parameter

**Method**: Add an optional output parameter `TokenUsage* out_usage = nullptr` to `parse_completion_response`. When provided, populate it with usage metrics.

**Implementation**:
1. Add parameter to signature:
```cpp
std::string parse_completion_response(
    const std::string& response_str,
    json& out_full_response,
    TokenUsage* out_usage = nullptr
) const;
```

2. Inside method, check if pointer is non-null and populate:
```cpp
if (out_usage && response.contains("usage")) {
    out_usage->prompt_tokens = response["usage"]["prompt_tokens"];
    out_usage->completion_tokens = response["usage"]["completion_tokens"];
    out_usage->total_tokens = response["usage"]["total_tokens"];
}
```

3. Callers can optionally pass `&usage_var` when they need metrics

**Advantages**: Backward compatible (optional param), explicit opt-in, type-safe
**Disadvantages**: C-style output parameter pattern (less modern C++)

## Rationale
{"anomaly": "Token usage data exists in API response but has no extraction mechanism", "approach": "Add optional output parameter for opt-in usage extraction", "alternatives_rejected": ["Always returning usage in pair (forces all callers to change)", "Global singleton to store usage (thread-safety issues, poor testability)"]}