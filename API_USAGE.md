# Loom API Usage Guide

## Quick Start

```bash
# Build and run the server
cd build
cmake --build . --target loom_api
./loom_api

# Server runs on http://localhost:8080
```

## Endpoints

### Health Check

```bash
curl http://localhost:8080/api/health
```
```json
{"status": "ok"}
```

---

### Sessions

#### Create Session

```bash
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "chatgpt",
    "model": "gpt-4o-mini",
    "verbose": false
  }'
```
```json
{
  "session_id": "sess-abc123",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z"
}
```

Engine options: `chatgpt`, `ollama`

#### List Sessions

```bash
curl http://localhost:8080/api/sessions
```
```json
{
  "sessions": [...],
  "count": 1
}
```

#### Get Session

```bash
curl http://localhost:8080/api/sessions/sess-abc123
```
```json
{
  "session_id": "sess-abc123",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z",
  "history_size": 2
}
```

#### Delete Session

```bash
curl -X DELETE http://localhost:8080/api/sessions/sess-abc123
```
```json
{"success": true, "message": "Session deleted"}
```

---

### Chat

#### Send Message

```bash
curl -X POST http://localhost:8080/api/sessions/sess-abc123/chat \
  -H "Content-Type: application/json" \
  -d '{
    "message": "Hello, how are you?",
    "system_prompt": "You are a helpful assistant.",
    "temperature": 0.7,
    "max_tokens": 500
  }'
```
```json
{
  "success": true,
  "message": "I'm doing well, thank you for asking!"
}
```

Only `message` is required. Other fields are optional.

If the agent calls any tools during execution, the response includes a `tool_calls` array:

```json
{
  "success": true,
  "message": "The answer is 42.",
  "tool_calls": [
    {
      "tool_name": "calculator",
      "arguments": {"operation": "multiply", "a": 6, "b": 7},
      "result": "Calculated: 6 * 7 = 42",
      "success": true
    }
  ]
}
```

#### Get History

```bash
curl http://localhost:8080/api/sessions/sess-abc123/history
```
```json
{
  "session_id": "sess-abc123",
  "message_count": 3,
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello"},
    {"role": "assistant", "content": "Hi there!"}
  ]
}
```

#### Clear History

```bash
curl -X DELETE http://localhost:8080/api/sessions/sess-abc123/history
```
```json
{"success": true, "message": "History cleared"}
```

---

### Tools

#### List Available Tools

```bash
curl http://localhost:8080/api/tools/available
```
```json
{
  "tools": [
    {"name": "base64", "description": "Encode and decode Base64 strings"},
    {"name": "web_search", "description": "Search the web using Brave Search API"},
    {"name": "web_fetch", "description": "Fetch and read web page content"},
    {"name": "github", "description": "Interact with GitHub repositories"},
    {"name": "calculator", "description": "Perform basic arithmetic operations"}
  ]
}
```

#### Get Session Tools

```bash
curl http://localhost:8080/api/sessions/sess-abc123/tools
```
```json
{
  "session_id": "sess-abc123",
  "tools": [
    {"name": "calculator", "description": "Perform basic arithmetic operations"}
  ],
  "tool_count": 1
}
```

#### Register Tools

```bash
curl -X POST http://localhost:8080/api/sessions/sess-abc123/tools \
  -H "Content-Type: application/json" \
  -d '{
    "tools": ["calculator", "web_search", "base64"]
  }'
```
```json
{
  "session_id": "sess-abc123",
  "registered_tools": ["calculator", "web_search", "base64"],
  "tool_count": 3
}
```

If any tools are invalid, they will be listed in `failed_tools`:
```json
{
  "session_id": "sess-abc123",
  "registered_tools": ["calculator"],
  "tool_count": 1,
  "failed_tools": ["invalid_tool"]
}
```

#### Unregister Tool

```bash
curl -X DELETE http://localhost:8080/api/sessions/sess-abc123/tools/calculator
```
```json
{
  "session_id": "sess-abc123",
  "removed_tool": "calculator",
  "success": true
}
```

#### Register Agent as Tool

Register another session's agent as a tool, enabling multi-agent workflows.

```bash
curl -X POST http://localhost:8080/api/sessions/sess-main456/tools/agent \
  -H "Content-Type: application/json" \
  -d '{
    "source_session_id": "sess-researcher123",
    "tool_name": "researcher",
    "tool_description": "Agent that can search the web"
  }'
```
```json
{
  "session_id": "sess-main456",
  "registered_agent_tool": "researcher",
  "source_session_id": "sess-researcher123",
  "success": true
}
```

Only `source_session_id` is required. `tool_name` and `tool_description` are optional.

**Note**: When an agent calls another agent as a tool, a **child session** is automatically created with isolated conversation history. This means:
- The child agent maintains its own conversation context across multiple calls
- The parent agent only sees the final result of each tool call
- Child sessions are automatically cleaned up when the parent session is destroyed

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `LOOM_API_PORT` | Server port (default: 8080) |
| `OPENAI_API_KEY` | Required for ChatGPT engine |
| `BRAVE_SEARCH_API_KEY` | Required for WebSearchTool |
| `GITHUB_TOKEN` | Required for GitHubTool |

---

## Example: Complete Chat Session

```bash
# 1. Create session
SESSION=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Session: $SESSION"

# 2. Send message
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "What is 2+2?"}'

# 3. Continue conversation
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "And what is that multiplied by 3?"}'

# 4. Get full history
curl http://localhost:8080/api/sessions/$SESSION/history

# 5. Cleanup
curl -X DELETE http://localhost:8080/api/sessions/$SESSION
```

---

## Example: Chat Session with Tools

```bash
# 1. Create session
SESSION=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Session: $SESSION"

# 2. Register tools
curl -X POST http://localhost:8080/api/sessions/$SESSION/tools \
  -H "Content-Type: application/json" \
  -d '{"tools": ["calculator", "web_search"]}'

# 3. Send message that uses tools
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "What is 15 * 7?"}'

# 4. Check registered tools
curl http://localhost:8080/api/sessions/$SESSION/tools

# 5. Cleanup
curl -X DELETE http://localhost:8080/api/sessions/$SESSION
```

---

## Example: Multi-Agent Workflow

```bash
# 1. Create a researcher agent with web tools
RESEARCHER=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

curl -X POST http://localhost:8080/api/sessions/$RESEARCHER/tools \
  -H "Content-Type: application/json" \
  -d '{"tools": ["web_search", "web_fetch"]}'

echo "Researcher: $RESEARCHER"

# 2. Create main orchestrator agent
MAIN=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Main: $MAIN"

# 3. Register researcher as a tool for main agent
curl -X POST http://localhost:8080/api/sessions/$MAIN/tools/agent \
  -H "Content-Type: application/json" \
  -d "{
    \"source_session_id\": \"$RESEARCHER\",
    \"tool_name\": \"researcher\",
    \"tool_description\": \"Agent that can search and fetch web content\"
  }"

# 4. Main agent can now delegate research tasks
curl -X POST http://localhost:8080/api/sessions/$MAIN/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "Use the researcher to find the latest news about AI safety"}'

# 5. Cleanup
curl -X DELETE http://localhost:8080/api/sessions/$MAIN
curl -X DELETE http://localhost:8080/api/sessions/$RESEARCHER
```

---

## Using with Ollama

```bash
# Make sure Ollama is running
ollama serve

# Create Ollama session
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "ollama",
    "model": "llama3.1:8b"
  }'
```
