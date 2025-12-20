# Token Counting Implementation: Optional Output Parameter Approach

## Context
**Problem Statement**: The OpenAI API returns token usage metrics (prompt_tokens, completion_tokens, total_tokens) in the response "usage" field, but the current implementation of parse_completion_response() only extracts the completion text. This prevents:
- Cost tracking and budgeting for API usage
- Performance monitoring and optimization
- Context window management validation
- API usage analytics and reporting

**User Request**: "How should I implement token counting for the OpenAI response structure?"

**Bounded Context**: C++20 tool system using CompletionEngine abstraction over multiple LLM APIs (ChatGPT, Gemini, Ollama). Must maintain type safety, backward compatibility, and handle errors gracefully. Uses nlohmann/json for JSON operations.

## Decision
**Selected Option:** add-tokenusage-as-output-parameter

**Selected Solution**: Add TokenUsage struct with optional output parameter to parse_completion_response()

We chose to implement token counting by:
1. Adding a TokenUsage struct (3 int fields: prompt_tokens, completion_tokens, total_tokens) to CompletionEngine.hpp
2. Extending parse_completion_response() signature with optional parameter: `TokenUsage* out_usage = nullptr`
3. Extracting usage metrics from API response when the parameter is provided
4. Maintaining full backward compatibility by making the parameter optional with default nullptr

**Implementation Files**:
- include/CompletionEngine.hpp (struct + signature update)
- src/CompletionEngine.cpp (extraction logic with null-safety)
- examples/token_usage_validation.cpp (comprehensive test suite)
- CMakeLists.txt (test target configuration)

## Rationale
**Why This Solution Won**:

**1. Effective Reliability (R_eff): 4/4 (HIGH)**
- Achieved highest trust calculus score with zero penalties
- Congruence Level = 3 (maximum): Direct evidence in target environment
- All 4 validation tests passed: backward compat, extraction, null safety, edge cases
- Formal Verifiability = 3/4: Type-safe with compile-time guarantees

**2. Backward Compatibility (Critical Requirement)**
- Zero breaking changes to existing code
- Agent.cpp:292 continues working without modification
- Optional parameter means gradual adoption without forced migration
- Existing call sites compile and run unchanged (verified empirically)

**3. Balanced Trade-offs**
- Type safety: Strongly-typed struct prevents runtime errors
- Opt-in design: Callers only pay for what they use
- Simplicity: Straightforward implementation (~20 lines of code)
- Standard C++ idiom: Output parameters well-understood pattern

**4. Comparison with Alternatives** (from 5 hypotheses evaluated):
- vs. Return pair/tuple: Avoids breaking changes (this was critical)
- vs. Use existing out_full_response: Provides type safety this approach lacks
- vs. Wrapper class: Achieves 90% of benefit with 10% of refactoring cost
- vs. Observer pattern: Enables inline business logic use (callbacks cannot)

**5. Low Bias Assessment**:
- Pet Idea Bias: LOW (5 alternatives considered with diversity constraint)
- NIH Bias: NONE (uses standard C++ patterns, no reinvention)
- Confirmation Bias: LOW (tests designed to falsify, edge cases included)

**6. Risk Profile**: Minimal residual risks, all mitigated
- Live API variance: Field-by-field extraction with existence checks
- Pointer lifetime: Standard C++ idiom, documented
- Thread safety: Appropriate for expected usage pattern
- Future extensibility: Struct can grow without breaking changes

### Characteristic Space (C.16)
**Quality Characteristics (C.16 Scoring)**:

| Characteristic | Score | Justification |
|----------------|-------|---------------|
| **Backward Compatibility** | A+ | Zero breaking changes, verified empirically |
| **Type Safety** | A | Compile-time checking via struct, null-pointer guarded |
| **Reliability** | A | R_eff = 4/4, comprehensive test coverage |
| **Maintainability** | A | Simple implementation, standard patterns |
| **Performance** | A | Zero overhead when not used, minimal when used |
| **Testability** | A | 4/4 tests passing, edge cases covered |
| **Extensibility** | A | Struct can grow, method can be overridden |
| **Discoverability** | B+ | Requires reading docs, but signature hints usage |
| **Ergonomics** | B | Requires pointer syntax vs. cleaner alternatives |
| **Modernity** | B- | C-style parameter vs. modern C++ return styles |

**Overall Grade**: **A** (Excellent for production use)

## Consequences
**Positive Consequences**:
✓ Token tracking immediately available for cost monitoring
✓ Zero disruption to existing codebase (backward compatible)
✓ Type-safe interface prevents common errors
✓ Enables gradual migration (callers opt-in when needed)
✓ Future-proof: Struct extensible for new fields (e.g., cached_tokens)
✓ Works across all CompletionEngine implementations (ChatGPT, Ollama, Gemini)
✓ Test suite provides regression protection

**Negative Consequences / Trade-offs**:
⚠ C-style output parameter (less modern than return value approach)
⚠ Caller responsible for pointer lifetime (standard C++ caveat)
⚠ Not inherently thread-safe (appropriate for per-request usage pattern)

**Action Items**:
1. **Immediate**: Merge implementation to main branch
2. **Short-term**: Update Agent.cpp to extract usage for cost tracking
3. **Optional**: Add integration test with live OpenAI API
4. **Documentation**: Add usage examples to README.md showing both patterns:
   - Legacy: `completion = engine->parse_completion_response(str, resp);`
   - New: `TokenUsage usage; completion = engine->parse_completion_response(str, resp, &usage);`

**Validity & Revisit Triggers**:
- **Revisit if**: OpenAI API changes usage field structure significantly
- **Revisit if**: C++23 adoption enables better alternatives (e.g., std::expected)
- **Revisit if**: Wrapper class approach becomes necessary for additional metadata
- **Review by**: 2026-06-20 (6 months) or when implementing streaming support
- **Monitor**: Real API usage in production for unexpected edge cases
