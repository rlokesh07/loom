---
scope: Conservative approach for codebases where backward compatibility is critical. Works with existing JSON parsing infrastructure. Suitable when token usage is optional/secondary concern.
kind: system
---

# Hypothesis: Store usage in out_full_response parameter

**Method**: Keep current signature but rely on the existing `out_full_response` parameter to contain token usage. Callers extract usage from this JSON object when needed.

**Implementation**:
1. No changes to `parse_completion_response` signature
2. Document that `out_full_response` contains the `usage` field
3. Callers access via: `out_full_response["usage"]["total_tokens"]`
4. Create optional helper method: `extract_token_usage(const json& response) -> TokenUsage`

**Advantages**: Non-breaking, backward compatible, minimal code changes
**Disadvantages**: Less discoverable, requires JSON navigation, no compile-time safety

## Rationale
{"anomaly": "Token usage metrics are lost because callers don't know to inspect out_full_response parameter", "approach": "Document and provide helper to extract usage from existing out_full_response JSON", "alternatives_rejected": ["Changing return type (too invasive for quick fix)", "Logging usage to stderr (not programmatically accessible)"]}