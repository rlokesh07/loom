---
scope: All CompletionEngine implementations (ChatGPT, Ollama, Gemini). Works across the entire Agent execution flow. Requires C++17 or higher for structured bindings convenience.
kind: system
---

# Hypothesis: Add TokenUsage struct to return value

**Method**: Create a `TokenUsage` struct to encapsulate token metrics, then modify `parse_completion_response` to return a pair/tuple of (completion_text, token_usage).

**Implementation**:
1. Define struct in CompletionEngine.hpp:
```cpp
struct TokenUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
};
```

2. Change signature to return `std::pair<std::string, TokenUsage>`
3. Parse the `usage` field from OpenAI response JSON
4. Update all call sites in Agent.cpp to handle the pair

**Advantages**: Type-safe, clean separation of concerns, standard C++ idiom
**Disadvantages**: Requires updating all existing callers (breaking change)

## Rationale
{"anomaly": "Token usage metrics from OpenAI API responses are currently ignored, preventing cost tracking and performance monitoring", "approach": "Return token usage as a separate strongly-typed struct alongside completion text", "alternatives_rejected": ["Global state to store usage (violates functional design)", "Ignoring the problem (fails business requirements for cost tracking)"]}