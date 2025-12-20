---
scope: Ideal for observability and monitoring systems where usage data feeds into separate analytics pipeline. Works well with event-driven architectures. Not suitable when caller needs usage for immediate decision-making.
kind: system
---

# Hypothesis: Logging-based usage tracking with observer pattern

**Method**: Implement a callback/observer pattern where token usage is automatically logged/tracked without changing return signatures.

**Implementation**:
1. Add optional callback to CompletionEngine:
```cpp
using UsageCallback = std::function<void(const TokenUsage&)>;
void set_usage_callback(UsageCallback cb);
```

2. Inside `parse_completion_response`, invoke callback if set:
```cpp
if (usage_callback_ && response.contains("usage")) {
    TokenUsage usage = extract_usage(response);
    usage_callback_(usage);
}
```

3. Applications can register callbacks for logging, metrics, cost tracking

**Advantages**: Zero-impact on callers, supports analytics/monitoring use cases, flexible
**Disadvantages**: Side effects during parsing, usage not available inline for business logic, requires state in CompletionEngine

## Rationale
{"anomaly": "Token usage metrics need to be collected system-wide for monitoring but shouldn't clutter business logic", "approach": "Observer pattern decouples usage collection from core execution flow", "alternatives_rejected": ["Direct logging to file (inflexible, hard-coded destination)", "Returning usage in-band (couples monitoring with business logic)"]}