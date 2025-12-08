# Loom API Usage Guide

## Quick Start

### Stateless Version (Recommended - with Supabase)

```bash
# 1. Set up Supabase (first time only)
# - Create project at https://supabase.com
# - Apply schema: migrations/001_initial_schema.sql in SQL editor
# - Get API credentials from Settings → API

# 2. Set environment variables
export SUPABASE_URL="https://your-project.supabase.co"
export SUPABASE_KEY="your-anon-key"
export OPENAI_API_KEY="sk-..."

# 3. Build and run
cd build
cmake --build . --target loom_api_stateless
./loom_api_stateless

# Server runs on http://localhost:8080
```

**Benefits of stateless version:**
- ✅ Data persists across server restarts
- ✅ Horizontal scaling ready (run multiple instances)
- ✅ All state in Supabase database
- ✅ Production-ready architecture

### Original Version (In-Memory, Stateful)

```bash
# Build and run the server
cd build
cmake --build . --target loom_api
./loom_api

# Server runs on http://localhost:8080
```

**Note:** Stateful version loses all sessions on restart. Use for quick local testing only.

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

### Required for Stateless Version

| Variable | Description |
|----------|-------------|
| `SUPABASE_URL` | Your Supabase project URL (e.g., `https://xxx.supabase.co`) |
| `SUPABASE_KEY` | Your Supabase anon/public API key |
| `OPENAI_API_KEY` | Required for ChatGPT engine |

### Optional

| Variable | Description | Default |
|----------|-------------|---------|
| `LOOM_API_PORT` | Server port | 8080 |
| `LOOM_CLIENT_ID` | Client identifier for multi-tenancy | "default-client" |
| `BRAVE_SEARCH_API_KEY` | Required for WebSearchTool | - |
| `GITHUB_TOKEN` | Required for GitHubTool | - |

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

---

## Stateless Architecture Details

### How It Works

The stateless version follows this pattern for every request:

1. **Load State from Database**
   - Session metadata (engine, model, system_prompt)
   - Conversation history (all messages)
   - Tool configuration (registered tool names)

2. **Reconstruct Runtime Objects**
   - Create fresh `CompletionEngine` (ChatGPT/Ollama/Gemini)
   - Create fresh `Agent` instance
   - Build `ToolRegistry` from tool names
   - Load conversation history into agent

3. **Execute**
   - Agent processes request with loaded context
   - Agent can use registered tools

4. **Save State Back to Database**
   - Append new user message
   - Append new assistant response
   - Update last_accessed_at timestamp

5. **Clean Up**
   - Agent instance destroyed
   - All runtime objects released
   - No state persists in server memory

### Testing Stateless Behavior

**Test 1: Persistence Across Restarts**
```bash
# Create session and send message
SESSION=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"Remember the number 42"}'

# Stop server (Ctrl+C)
# Restart server
# Query history - data should still be there!
curl http://localhost:8080/api/sessions/$SESSION/history
```

**Test 2: Multiple Server Instances (Horizontal Scaling)**
```bash
# Terminal 1
export LOOM_API_PORT=8080
./loom_api_stateless

# Terminal 2
export LOOM_API_PORT=8081
./loom_api_stateless

# Terminal 3 - Test
SESSION=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

# Send message to server 1
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"Hello from server 1"}'

# Query from server 2 - should see the same data!
curl http://localhost:8081/api/sessions/$SESSION/history
```

### Database Schema

The stateless version uses these Supabase tables:

**sessions**
- `session_id` (UUID, primary key)
- `client_id` (UUID, for multi-tenancy)
- `engine_type` (chatgpt/ollama/gemini)
- `model` (model name)
- `system_prompt` (optional system instructions)
- `conversation_history` (JSONB array of messages)
- `tool_names` (TEXT[] array of registered tools)
- `created_at`, `last_accessed_at` (timestamps)

**child_sessions** (for agent-as-tool support)
- `parent_session_id` (UUID)
- `tool_name` (TEXT)
- `child_session_id` (UUID)

**clients** (for multi-tenancy - future use)
- `client_id` (UUID, primary key)
- `api_key_hash` (for authentication)
- `display_name`, quotas, timestamps

### Viewing Data in Supabase

Go to your Supabase project dashboard:
1. Click **Table Editor**
2. Select **sessions** table
3. See all session data including conversation_history JSONB
4. Query, filter, and inspect data directly

---

## Troubleshooting

### "Database connection failed"
- Verify `SUPABASE_URL` and `SUPABASE_KEY` are set correctly
- Check you've applied the schema (`migrations/001_initial_schema.sql`)
- Verify internet connection

### "Session not found"
- Ensure you're using the correct `session_id`
- Check the session exists in Supabase dashboard
- Verify you're using the same `LOOM_CLIENT_ID`

### Messages not persisting
- Check the stateless version is running (not the stateful version)
- Verify `add_message` calls are completing successfully
- Inspect `conversation_history` column in Supabase dashboard

---

## Migration from Stateful to Stateless

If you have existing code using the old stateful API, the endpoints are identical. You only need to:

1. Set up Supabase and apply schema
2. Set `SUPABASE_URL` and `SUPABASE_KEY` environment variables
3. Use `loom_api_stateless` instead of `loom_api`

All API requests work exactly the same way!
