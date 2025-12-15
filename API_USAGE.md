# Loom API Usage Guide

**Last Updated**: 2025-12-15 (Updated for OpenAI-compatible endpoints)

**Production URL**: `https://loom-production.up.railway.app`

## Quick Start

### Using Production API (Recommended)

The Loom API is deployed and ready to use at:
```
https://loom-production.up.railway.app
```

No setup required! Just create a client and start using the API:

```bash
# 1. Create a client to get your API key
API_KEY=$(curl -s -X POST https://loom-production.up.railway.app/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My Application"}' | jq -r '.api_key')

echo "Your API Key: $API_KEY"
echo "⚠️ Save this key - it won't be shown again!"

# 2. Create a session and start chatting
SESSION=$(curl -s -X POST https://loom-production.up.railway.app/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

echo "Session ID: $SESSION"

# 3. Send a message
curl -X POST https://loom-production.up.railway.app/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message":"Hello! What can you help me with?"}'
```

---

## OpenAI-Compatible API

Loom now supports **OpenAI-compatible endpoints**, allowing you to use any OpenAI SDK or client library with Loom's multi-tenant architecture.

### Key Features

- **Session-Specific API Keys**: Each session gets a unique API key (e.g., `sk-session-abc123...`)
- **OpenAI SDK Compatible**: Works with OpenAI Python SDK, Node.js SDK, or any OpenAI-compatible client
- **Dual Mode Operation**:
  - **Session Mode**: Use session keys for stateful conversations with history
  - **Stateless Mode**: Use base keys for one-off completions without history
- **30+ Supported Models**: GPT-4, GPT-3.5, Gemini, Llama3, Mistral, and more

### Quick Start with OpenAI Compatibility

```bash
# 1. Create a client to get your base API key
BASE_KEY=$(curl -s -X POST https://loom-production.up.railway.app/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}' | jq -r '.api_key')

echo "Base API Key: $BASE_KEY"

# 2. Create a session to get a session-specific API key
RESPONSE=$(curl -s -X POST https://loom-production.up.railway.app/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $BASE_KEY" \
  -d '{"engine":"chatgpt","model":"gpt-3.5-turbo"}')

SESSION_KEY=$(echo $RESPONSE | jq -r '.session_api_key')
SESSION_ID=$(echo $RESPONSE | jq -r '.session_id')

echo "Session API Key: $SESSION_KEY"
echo "Session ID: $SESSION_ID"

# 3. Use the session key with OpenAI-compatible endpoint
curl -X POST https://loom-production.up.railway.app/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_KEY" \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "user", "content": "Hello! Tell me a joke."}
    ]
  }'
```

### Using with OpenAI Python SDK

```python
from openai import OpenAI

# Option 1: Session Mode (stateful, maintains history)
client = OpenAI(
    api_key="sk-session-abc123...",  # Use session-specific key
    base_url="https://loom-production.up.railway.app/v1"
)

response = client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "user", "content": "Hello!"}
    ]
)

print(response.choices[0].message.content)

# Continue the conversation - history is maintained
response = client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "user", "content": "What did I just say?"}
    ]
)

# Option 2: Stateless Mode (no history, one-off completion)
client = OpenAI(
    api_key="sk-abc123...",  # Use base key
    base_url="https://loom-production.up.railway.app/v1"
)

response = client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is 2+2?"}
    ]
)
```

### Using with OpenAI Node.js SDK

```javascript
const OpenAI = require('openai');

// Session Mode
const client = new OpenAI({
  apiKey: 'sk-session-abc123...',
  baseURL: 'https://loom-production.up.railway.app/v1'
});

const response = await client.chat.completions.create({
  model: 'gpt-3.5-turbo',
  messages: [
    { role: 'user', content: 'Hello!' }
  ]
});

console.log(response.choices[0].message.content);
```

### OpenAI Endpoints

#### POST /v1/chat/completions

Standard OpenAI chat completions endpoint with dual-mode operation.

**Session Mode** (use session key):
```bash
curl -X POST $API_BASE_URL/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-session-abc123..." \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "user", "content": "Hello!"}
    ],
    "temperature": 0.7,
    "max_tokens": 500
  }'
```

**Stateless Mode** (use base key):
```bash
curl -X POST $API_BASE_URL/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-abc123..." \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "What is 2+2?"}
    ]
  }'
```

**Response** (OpenAI-compatible):
```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1234567890,
  "model": "gpt-3.5-turbo",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello! How can I assist you today?"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 0,
    "completion_tokens": 0,
    "total_tokens": 0
  }
}
```

**Session Mode Behavior**:
- Conversation history is automatically maintained
- New messages are intelligently deduplicated (O(1) performance)
- Each request appends to the session's history
- Messages persist across requests

**Stateless Mode Behavior**:
- No history is saved
- Each request is independent
- You must include full conversation context in each request
- Useful for one-off completions or custom history management

#### GET /v1/models

List all available models and their engine mappings.

```bash
curl $API_BASE_URL/v1/models
```

Response:
```json
{
  "object": "list",
  "data": [
    {
      "id": "gpt-4",
      "object": "model",
      "created": 1234567890,
      "owned_by": "openai",
      "loom_engine": "chatgpt"
    },
    {
      "id": "gpt-4-turbo",
      "object": "model",
      "created": 1234567890,
      "owned_by": "openai",
      "loom_engine": "chatgpt"
    },
    {
      "id": "gemini-pro",
      "object": "model",
      "created": 1234567890,
      "owned_by": "google",
      "loom_engine": "gemini"
    }
  ]
}
```

### Session-Specific API Keys

When you create a session, Loom automatically generates a session-specific API key:

```bash
# Create session with base key
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-abc123..." \
  -d '{"engine":"chatgpt","model":"gpt-3.5-turbo"}'
```

Response:
```json
{
  "session_id": "123e4567-e89b-12d3-a456-426614174000",
  "session_api_key": "sk-session-abc123def456...",
  "engine": "chatgpt",
  "model": "gpt-3.5-turbo",
  "created_at": "2025-12-15T10:30:00Z"
}
```

**Key Types**:
- **Base Key** (`sk-abc...`): Can create sessions, access all OpenAI endpoints
- **Session Key** (`sk-session-abc...`): Scoped to specific session, for stateful conversations

**Security**:
- Session keys cannot create new sessions (403 error)
- Session keys are automatically scoped to their session
- Base keys required for session management operations

### Supported Models

**OpenAI (engine: chatgpt)**
- `gpt-4`, `gpt-4-turbo`, `gpt-4o`, `gpt-4o-mini`
- `gpt-3.5-turbo`, `gpt-3.5-turbo-16k`
- `o1-preview`, `o1-mini`, `o3-mini`

**Google (engine: gemini)**
- `gemini-pro`, `gemini-1.5-pro`, `gemini-1.5-flash`
- `gemini-1.0-pro`, `gemini-2.0-flash-exp`

**Ollama (engine: ollama)**
- `llama3`, `llama3.1`, `llama3.2`, `llama3:70b`
- `mistral`, `mixtral:8x7b`, `codellama`, `deepseek-coder`
- `qwen2.5-coder`, `phi3`, `gemma2`

**Note**: Requires appropriate API keys configured on server:
- `OPENAI_API_KEY` for ChatGPT models
- `GEMINI_API_KEY` for Gemini models
- Ollama running locally for Ollama models

---

## Authentication

**⚠️ IMPORTANT**: All endpoints (except `/api/health` and `/api/clients`) now require authentication using an API key.

### 1. Create a Client (Get API Key)

First, create a client to get an API key:

```bash
curl -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "My Application"
  }'
```

Response:
```json
{
  "client_id": "123e4567-e89b-12d3-a456-426614174000",
  "api_key": "sk-a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6",
  "message": "Save this API key - it won't be shown again"
}
```

**⚠️ CRITICAL**: Save the `api_key` immediately! It's only shown once and cannot be retrieved later.

### 2. Use API Key in Requests

Include your API key in the `Authorization` header for all subsequent requests:

```bash
curl -H "Authorization: Bearer sk-a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6" \
  $API_BASE_URL/api/sessions
```

### 3. Get Current Client Info

```bash
curl $API_BASE_URL/api/clients/me \
  -H "Authorization: Bearer sk-a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6"
```

```json
{
  "client_id": "123e4567-e89b-12d3-a456-426614174000"
}
```

---

## Endpoints

**For all examples below, we'll use the production URL. If you're self-hosting, replace with your local URL:**

```bash
# Using production API (recommended)
export API_BASE_URL="https://loom-production.up.railway.app"

# OR if self-hosting locally
# export API_BASE_URL="http://localhost:8080"
```

### Health Check (No Auth Required)

```bash
curl $API_BASE_URL/api/health
```
```json
{"status": "ok"}
```

---

### Sessions

**Note**: Set your API key as an environment variable for convenience:
```bash
# Get your API key first (see Quick Start above)
export API_KEY="sk-a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6"
```

#### Create Session

```bash
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "engine": "chatgpt",
    "model": "gpt-4o-mini",
    "verbose": false
  }'
```
```json
{
  "session_id": "sess-abc123",
  "session_api_key": "sk-session-abc123def456...",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z"
}
```

Engine options: `chatgpt`, `ollama`, `gemini`

**Note**: The `session_api_key` is automatically generated and can be used with OpenAI-compatible endpoints for this specific session.

#### List Sessions

```bash
curl $API_BASE_URL/api/sessions \
  -H "Authorization: Bearer $API_KEY"
```
```json
{
  "sessions": [...],
  "count": 1
}
```

#### Get Session

```bash
curl $API_BASE_URL/api/sessions/sess-abc123 \
  -H "Authorization: Bearer $API_KEY"
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
curl -X DELETE $API_BASE_URL/api/sessions/sess-abc123 \
  -H "Authorization: Bearer $API_KEY"
```
```json
{"success": true, "message": "Session deleted"}
```

---

### Chat

#### Send Message

```bash
curl -X POST $API_BASE_URL/api/sessions/sess-abc123/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
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
curl $API_BASE_URL/api/sessions/sess-abc123/history \
  -H "Authorization: Bearer $API_KEY"
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
curl -X DELETE $API_BASE_URL/api/sessions/sess-abc123/history \
  -H "Authorization: Bearer $API_KEY"
```
```json
{"success": true, "message": "History cleared"}
```

---

### Tools

#### List Available Tools

```bash
curl $API_BASE_URL/api/tools/available \
  -H "Authorization: Bearer $API_KEY"
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
curl $API_BASE_URL/api/sessions/sess-abc123/tools \
  -H "Authorization: Bearer $API_KEY"
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
curl -X POST $API_BASE_URL/api/sessions/sess-abc123/tools \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
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
curl -X DELETE $API_BASE_URL/api/sessions/sess-abc123/tools/calculator \
  -H "Authorization: Bearer $API_KEY"
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
curl -X POST $API_BASE_URL/api/sessions/sess-main456/tools/agent \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
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
| `BRAVE_SEARCH_API_KEY` | Required for WebSearchTool | - |
| `GITHUB_TOKEN` | Required for GitHubTool | - |
| `GEMINI_API_KEY` | Required for Gemini engine | - |

**Note**: `LOOM_CLIENT_ID` is no longer used. Multi-tenancy is now handled via API key authentication.

---

## Example: Complete Chat Session

```bash
# 0. Create client and get API key (only do this once!)
API_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}' | jq -r '.api_key')

echo "API Key: $API_KEY"
echo "⚠️ Save this key - it won't be shown again!"

# 1. Create session
SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Session: $SESSION"

# 2. Send message
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "What is 2+2?"}'

# 3. Continue conversation
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "And what is that multiplied by 3?"}'

# 4. Get full history
curl $API_BASE_URL/api/sessions/$SESSION/history \
  -H "Authorization: Bearer $API_KEY"

# 5. Cleanup
curl -X DELETE $API_BASE_URL/api/sessions/$SESSION \
  -H "Authorization: Bearer $API_KEY"
```

---

## Example: Chat Session with Tools

```bash
# Assuming you already have API_KEY from client creation
# If not, create client first (see previous example)

# 1. Create session
SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Session: $SESSION"

# 2. Register tools
curl -X POST $API_BASE_URL/api/sessions/$SESSION/tools \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"tools": ["calculator", "web_search"]}'

# 3. Send message that uses tools
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "What is 15 * 7?"}'

# 4. Check registered tools
curl $API_BASE_URL/api/sessions/$SESSION/tools \
  -H "Authorization: Bearer $API_KEY"

# 5. Cleanup
curl -X DELETE $API_BASE_URL/api/sessions/$SESSION \
  -H "Authorization: Bearer $API_KEY"
```

---

## Example: Multi-Agent Workflow

```bash
# Assuming you already have API_KEY from client creation

# 1. Create a researcher agent with web tools
RESEARCHER=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

curl -X POST $API_BASE_URL/api/sessions/$RESEARCHER/tools \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"tools": ["web_search", "web_fetch"]}'

echo "Researcher: $RESEARCHER"

# 2. Create main orchestrator agent
MAIN=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Main: $MAIN"

# 3. Register researcher as a tool for main agent
curl -X POST $API_BASE_URL/api/sessions/$MAIN/tools/agent \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d "{
    \"source_session_id\": \"$RESEARCHER\",
    \"tool_name\": \"researcher\",
    \"tool_description\": \"Agent that can search and fetch web content\"
  }"

# 4. Main agent can now delegate research tasks
curl -X POST $API_BASE_URL/api/sessions/$MAIN/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "Use the researcher to find the latest news about AI safety"}'

# 5. Cleanup
curl -X DELETE $API_BASE_URL/api/sessions/$MAIN \
  -H "Authorization: Bearer $API_KEY"
curl -X DELETE $API_BASE_URL/api/sessions/$RESEARCHER \
  -H "Authorization: Bearer $API_KEY"
```

---

## Using with Ollama

```bash
# Make sure Ollama is running
ollama serve

# Create Ollama session (assuming you have API_KEY)
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "engine": "ollama",
    "model": "llama3.1:8b"
  }'
```

---

## Using with Gemini

```bash
# Make sure GEMINI_API_KEY is set in server environment

# Create Gemini session (assuming you have API_KEY)
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "engine": "gemini",
    "model": "gemini-pro"
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
# Create client and get API key
API_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"Test"}' | jq -r '.api_key')

# Create session and send message
SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message":"Remember the number 42"}'

# Stop server (Ctrl+C)
# Restart server
# Query history - data should still be there!
curl $API_BASE_URL/api/sessions/$SESSION/history \
  -H "Authorization: Bearer $API_KEY"
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
# Create client (only once, can use either server)
API_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"Test"}' | jq -r '.api_key')

SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

# Send message to server 1
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message":"Hello from server 1"}'

# Query from server 2 - should see the same data!
curl http://localhost:8081/api/sessions/$SESSION/history \
  -H "Authorization: Bearer $API_KEY"

# Note: The above test is for self-hosted multi-instance deployment.
# For Railway production, all requests go through the same URL.
```

**Test 3: Multi-Tenancy Isolation**
```bash
# Create two separate clients
CLIENT_A_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"Client A"}' | jq -r '.api_key')

CLIENT_B_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"Client B"}' | jq -r '.api_key')

# Client A creates a session
SESSION_A=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $CLIENT_A_KEY" \
  -d '{"engine":"chatgpt","model":"gpt-4o-mini"}' | jq -r '.session_id')

# Client B tries to access Client A's session - should fail with 404
curl $API_BASE_URL/api/sessions/$SESSION_A \
  -H "Authorization: Bearer $CLIENT_B_KEY"

# Expected: {"error": "Session not found"} or 404

# Client A can access their own session - should succeed
curl $API_BASE_URL/api/sessions/$SESSION_A \
  -H "Authorization: Bearer $CLIENT_A_KEY"
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

**api_key_aliases** (for API key management)
- `id` (SERIAL, primary key)
- `key_hash` (TEXT, SHA-256 hash of API key, unique)
- `client_id` (UUID, references clients)
- `session_id` (UUID, optional, references sessions)
- `key_type` (TEXT, either 'base' or 'session')
- `created_at`, `last_used_at` (timestamps)
- `is_active` (BOOLEAN, for key revocation)

**child_sessions** (for agent-as-tool support)
- `parent_session_id` (UUID)
- `tool_name` (TEXT)
- `child_session_id` (UUID)

**clients** (for multi-tenancy authentication)
- `client_id` (UUID, primary key)
- `display_name` (optional client name)
- `credentials_encrypted` (JSONB, encrypted per-client credentials for tools)
- `max_sessions`, `max_requests_per_hour` (quotas)
- `created_at`, `last_active_at` (timestamps)

**Note**: The `api_key_hash` column was removed from the `clients` table. All API keys (both base and session-specific) are now stored in the `api_key_aliases` table for better management and scalability.

### Viewing Data in Supabase

Go to your Supabase project dashboard:
1. Click **Table Editor**
2. Select **sessions** table
3. See all session data including conversation_history JSONB
4. Query, filter, and inspect data directly

---

## Troubleshooting

### "Authorization header required" (401)
- You forgot to include the `Authorization` header
- Add `-H "Authorization: Bearer $API_KEY"` to your curl request
- Make sure you created a client first with `POST /api/clients`

### "Invalid or expired API key" (401)
- The API key is incorrect or doesn't exist in the database
- Verify you're using the correct API key from client creation
- Create a new client if you lost your API key (it cannot be retrieved)

### "Session not found" (404)
- The session doesn't exist, or
- **You're using a different client's API key** (Row-Level Security prevents cross-client access)
- Verify the session belongs to the authenticated client

### "Database connection failed"
- Verify `SUPABASE_URL` and `SUPABASE_KEY` are set correctly
- Check you've applied all migrations in order (001 through 006)
- Verify internet connection
- Check Supabase project is active and not paused

### Messages not persisting
- Check the stateless version is running (not the stateful version)
- Verify `add_message` calls are completing successfully
- Inspect `conversation_history` column in Supabase dashboard
- Check RLS policies are properly configured (migration 004)

### Multi-tenancy not working
- Verify you applied migration 004 (`enable_multi_tenancy.sql`)
- Check RLS is enabled on sessions table: `SELECT tablename, rowsecurity FROM pg_tables WHERE tablename = 'sessions';`
- Verify policies exist: `SELECT * FROM pg_policies WHERE tablename = 'sessions';`

---

## Migration from Stateful to Stateless

If you have existing code using the old stateful API, here's what you need to update:

### 1. Set up Supabase
- Create Supabase project
- Apply all migrations (001 through 006) **in order**
  - Migration 006 adds the `api_key_aliases` table for OpenAI-compatible endpoints
- Set `SUPABASE_URL` and `SUPABASE_KEY` environment variables

### 2. Add Authentication
**⚠️ BREAKING CHANGE**: All endpoints now require authentication

**Old code** (stateful):
```bash
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}'
```

**New code** (stateless with auth):
```bash
# First create a client
API_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}' | jq -r '.api_key')

# Then add Authorization header to all requests
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}'
```

### 3. Update Environment Variables
- Remove: `LOOM_CLIENT_ID` (no longer used)
- Keep: `SUPABASE_URL`, `SUPABASE_KEY`, `OPENAI_API_KEY`, etc.

### 4. Use New Binary
- Build and use `loom_api_stateless` instead of `loom_api`

### Summary of Changes
- ✅ All state persists in database (survives restarts)
- ✅ Horizontally scalable (run multiple instances)
- ✅ OpenAI-compatible endpoints (`/v1/chat/completions`, `/v1/models`)
- ✅ Session-specific API keys for stateful conversations
- ✅ Dual-mode operation (session vs stateless)
- ⚠️ **REQUIRES**: `Authorization: Bearer <api-key>` header on all requests (except health check and client creation)
- ⚠️ **REQUIRES**: Creating a client first with `POST /api/clients`
- ✅ Multi-tenant with Row-Level Security
- ✅ Engine support: chatgpt, ollama, gemini
- ⚠️ **BREAKING**: The old `/api/sessions/{id}/chat` endpoint is still supported but `/v1/chat/completions` is the recommended OpenAI-compatible alternative

---

## Example: Using OpenAI Python SDK with Loom

This example shows how to use the official OpenAI Python SDK with Loom's OpenAI-compatible endpoints:

```python
from openai import OpenAI
import requests

# 1. Create a Loom client and get base API key
response = requests.post(
    "https://loom-production.up.railway.app/api/clients",
    json={"display_name": "My Python App"}
)
base_key = response.json()["api_key"]
print(f"Base API Key: {base_key}")
print("⚠️ Save this key - it won't be shown again!")

# 2. Create a session to get session-specific API key
session_response = requests.post(
    "https://loom-production.up.railway.app/api/sessions",
    headers={"Authorization": f"Bearer {base_key}"},
    json={"engine": "chatgpt", "model": "gpt-3.5-turbo"}
)
session_data = session_response.json()
session_key = session_data["session_api_key"]
session_id = session_data["session_id"]

print(f"Session ID: {session_id}")
print(f"Session Key: {session_key}")

# 3. Use OpenAI SDK with session key for stateful conversation
client = OpenAI(
    api_key=session_key,
    base_url="https://loom-production.up.railway.app/v1"
)

# First message
response = client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "user", "content": "My name is Alice. Remember it!"}
    ]
)
print("Assistant:", response.choices[0].message.content)

# Follow-up message - history is maintained automatically
response = client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "user", "content": "What's my name?"}
    ]
)
print("Assistant:", response.choices[0].message.content)
# Output: "Your name is Alice."

# 4. Alternatively, use base key for stateless mode
stateless_client = OpenAI(
    api_key=base_key,
    base_url="https://loom-production.up.railway.app/v1"
)

# Stateless completion - no history saved
response = stateless_client.chat.completions.create(
    model="gpt-3.5-turbo",
    messages=[
        {"role": "system", "content": "You are a math tutor."},
        {"role": "user", "content": "What is 15 * 7?"}
    ]
)
print("Assistant:", response.choices[0].message.content)
```

**Key Benefits**:
- Use familiar OpenAI SDK (no custom client needed)
- Session keys provide automatic conversation history
- Base keys for one-off completions without state
- Multi-tenant isolation built-in
- Works with any OpenAI-compatible tool or library
