---
scope: Best for long-term architecture where response complexity will grow. Suitable for applications needing rich response metadata (finish_reason, model info, latency). Requires C++11 minimum, benefits from C++17 move semantics.
kind: system
---

# Hypothesis: Response wrapper class with fluent interface

**Method**: Create a `CompletionResponse` class that encapsulates both text and metadata, providing a fluent interface for accessing different aspects.

**Implementation**:
1. Define wrapper class:
```cpp
class CompletionResponse {
    std::string text_;
    TokenUsage usage_;
    json full_response_;
public:
    const std::string& text() const;
    const TokenUsage& usage() const;
    const json& full_json() const;
    bool has_tool_calls() const;
    // ... other accessors
};
```

2. Return `CompletionResponse` from `parse_completion_response`
3. Provides extensibility for future metadata (model version, finish_reason, etc.)

**Advantages**: Highly extensible, encapsulation, future-proof for additional metadata
**Disadvantages**: Most invasive change, requires significant refactoring of Agent code

## Rationale
{"anomaly": "Current string return type cannot accommodate growing metadata needs (usage, finish_reason, model, timing)", "approach": "Introduce response object that can evolve with API changes", "alternatives_rejected": ["Tuple of multiple values (gets unwieldy with >3 fields)", "Multiple output parameters (poor API design)"]}