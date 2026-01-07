# Loom API Documentation

**Production URL**: `https://loom-production.up.railway.app`

A stateless, multi-tenant LLM API with OpenAI-compatible endpoints, built on Supabase.

---

## Quick Start

```bash
# 1. Create client and get API key
API_KEY=$(curl -s -X POST https://loom-production.up.railway.app/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}' | jq -r '.api_key')

echo "⚠️  Save this key: $API_KEY"

# 2. Create session (optionally with tools)
RESPONSE=$(curl -s -X POST https://loom-production.up.railway.app/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "engine": "chatgpt",
    "model": "gpt-4o-mini",
    "tools": ["calculator"]
  }')

SESSION_KEY=$(echo $RESPONSE | jq -r '.session_api_key')

# 3. Chat using OpenAI-compatible endpoint
curl -X POST https://loom-production.up.railway.app/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'
```

---

## OpenAI-Compatible API

Use the official OpenAI SDK with Loom's endpoints.

### Python

```python
from openai import OpenAI
import requests

# Create client and session (do this once)
base_key = requests.post(
    "https://loom-production.up.railway.app/api/clients",
    json={"display_name": "My App"}
).json()["api_key"]

session_key = requests.post(
    "https://loom-production.up.railway.app/api/sessions",
    headers={"Authorization": f"Bearer {base_key}"},
    json={
        "engine": "chatgpt",
        "model": "gpt-4o-mini",
        "system_prompt": "You are a helpful assistant with access to tools. Use them when needed.",
        "tools": ["calculator"]  # Optional: enable tools
    }
).json()["session_api_key"]

# Use OpenAI SDK with session key (stateful conversation with tools)
client = OpenAI(
    api_key=session_key,
    base_url="https://loom-production.up.railway.app/v1"
)

# Note: Session was created with tools=["calculator"], so agent can use calculator automatically

response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "Hello!"}]
)
print(response.choices[0].message.content)

# History is maintained automatically - just send new messages
response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "What did I just say?"}]
)
```

### JavaScript

```javascript
const OpenAI = require('openai');

const client = new OpenAI({
  apiKey: 'sk-session-abc123...',  // Session key from above
  baseURL: 'https://loom-production.up.railway.app/v1'
});

const response = await client.chat.completions.create({
  model: 'gpt-4o-mini',
  messages: [{ role: 'user', content: 'Hello!' }]
});
```

### Key Concepts

**Session Mode** (use session key `sk-session-...`):
- Conversation history maintained automatically
- Each message appends to session history
- Perfect for chatbots and assistants

**Stateless Mode** (use base key `sk-...`):
- No history saved
- Include full context in each request
- Perfect for one-off completions

---

## Authentication

All endpoints (except `/api/health` and `/api/clients`) require authentication:

```bash
-H "Authorization: Bearer <api-key>"
```

### Create Client

```bash
curl -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name": "My Application"}'
```

Response:
```json
{
  "client_id": "123e4567-e89b-12d3-a456-426614174000",
  "api_key": "sk-a1b2c3d4...",
  "message": "Save this API key - it won't be shown again"
}
```

---

## Core Endpoints

### Sessions

**Create Session**
```bash
POST /api/sessions
Headers: Authorization: Bearer <base-key>
Body: {
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "system_prompt": "You are a helpful assistant",
  "tools": ["calculator", "web_search"]  // Optional: register tools during creation
}
```

Response includes `session_api_key` for OpenAI-compatible endpoints.

All fields are optional except defaults:
- `engine` - Default: "chatgpt"
- `model` - Default: "gpt-3.5-turbo"
- `system_prompt` - Optional system message
- `tools` - Optional array of tool names to register immediately

**List Sessions**
```bash
GET /api/sessions
Headers: Authorization: Bearer <base-key>
```

**Get Session**
```bash
GET /api/sessions/{session_id}
Headers: Authorization: Bearer <base-key>
```

**Delete Session**
```bash
DELETE /api/sessions/{session_id}
Headers: Authorization: Bearer <base-key>
```

### Chat Completions

**OpenAI-Compatible Endpoint (Recommended)**
```bash
POST /v1/chat/completions
Headers: Authorization: Bearer <session-key>
Body: {
  "model": "gpt-4o-mini",
  "messages": [{"role": "user", "content": "Hello"}],
  "temperature": 0.7,
  "max_tokens": 500
}
```

When tools are registered to the session, the agent automatically uses them when needed.
The response follows OpenAI's format with tool calls handled internally - you receive
the final answer after all tool executions complete.

**Legacy Chat Endpoint**
```bash
POST /api/sessions/{session_id}/chat
Headers: Authorization: Bearer <base-key>
Body: {"message": "Hello"}
```

### Conversation History

**Get History**
```bash
GET /api/sessions/{session_id}/history
```

**Clear History**
```bash
DELETE /api/sessions/{session_id}/history
```

### Models

**List Available Models**
```bash
GET /v1/models
```

---

## Tools

### Available Tools

- `calculator` - Basic arithmetic operations
- `base64` - Encode/decode Base64 strings
- `web_search` - Search the web (requires `BRAVE_SEARCH_API_KEY`)
- `web_fetch` - Fetch web page content
- `github` - Interact with GitHub repositories (requires `GITHUB_TOKEN`)

### List Available Tools

```bash
GET /api/tools/available
```

### Register Tools to Session

**Option 1: During Session Creation (Recommended)**
```bash
POST /api/sessions
Body: {
  "model": "gpt-4o-mini",
  "tools": ["calculator", "web_search"]
}
```

**Option 2: Add Tools to Existing Session**
```bash
POST /api/sessions/{session_id}/tools
Body: {"tools": ["calculator", "web_search"]}
```

### List Session Tools

```bash
GET /api/sessions/{session_id}/tools
```

### Unregister Tool

```bash
DELETE /api/sessions/{session_id}/tools/{tool_name}
```

---

## Supported Models

### OpenAI (engine: `chatgpt`)
- `gpt-4`, `gpt-4-turbo`, `gpt-4o`, `gpt-4o-mini`
- `gpt-3.5-turbo`, `gpt-3.5-turbo-16k`
- `o1-preview`, `o1-mini`, `o3-mini`

### Google (engine: `gemini`)
- `gemini-pro`, `gemini-1.5-pro`, `gemini-1.5-flash`
- `gemini-2.0-flash-exp`

### Ollama (engine: `ollama`)
- `llama3`, `llama3.1`, `llama3.2`
- `mistral`, `mixtral:8x7b`
- `codellama`, `deepseek-coder`, `qwen2.5-coder`
- `phi3`, `gemma2`

---

## Environment Variables

### Required

| Variable | Description |
|----------|-------------|
| `SUPABASE_URL` | Supabase project URL |
| `SUPABASE_KEY` | Supabase anon/public key |
| `OPENAI_API_KEY` | Required for ChatGPT models |

### Optional

| Variable | Description | Default |
|----------|-------------|---------|
| `LOOM_API_PORT` | Server port | 8080 |
| `GEMINI_API_KEY` | Required for Gemini models | - |
| `BRAVE_SEARCH_API_KEY` | Required for web_search tool | - |
| `GITHUB_TOKEN` | Required for github tool | - |

---

## Examples

### Complete Chat Session

```bash
# Create client
API_KEY=$(curl -s -X POST $API_BASE_URL/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}' | jq -r '.api_key')

# Create session
SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

# Send messages
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "What is 2+2?"}'

curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "Multiply that by 3"}'

# Get history
curl $API_BASE_URL/api/sessions/$SESSION/history \
  -H "Authorization: Bearer $API_KEY"
```

### Session with Tools

**Method 1: Register Tools During Session Creation (Recommended)**

```bash
# Create session with tools in one step
RESPONSE=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "system_prompt": "You are a helpful assistant with access to tools. Use them when needed.",
    "tools": ["calculator", "web_search"]
  }')

SESSION_KEY=$(echo $RESPONSE | jq -r '.session_api_key')

# Send message using OpenAI-compatible endpoint - agent can use tools automatically
curl -X POST $API_BASE_URL/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $SESSION_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "What is 42 times 17?"}]
  }'
```

**Method 2: Add Tools to Existing Session**

```bash
# Create session first
SESSION=$(curl -s -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

# Then register tools
curl -X POST $API_BASE_URL/api/sessions/$SESSION/tools \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"tools": ["calculator", "web_search"]}'

# Send message - agent can use tools
curl -X POST $API_BASE_URL/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"message": "What is 15 * 7?"}'
```

If tools are used, response includes `tool_calls` array:
```json
{
  "success": true,
  "message": "The answer is 105.",
  "tool_calls": [
    {
      "tool_name": "calculator",
      "arguments": {"operation": "multiply", "a": 15, "b": 7},
      "result": "Calculated: 15 * 7 = 105",
      "success": true
    }
  ]
}
```

### Using Gemini

```bash
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "gemini", "model": "gemini-pro"}'
```

### Using Ollama

```bash
# Ensure Ollama is running locally
curl -X POST $API_BASE_URL/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"engine": "ollama", "model": "llama3.1"}'
```

---

## Architecture

### Stateless Design

Every request follows this flow:

1. **Load** - Fetch session state from Supabase (metadata, history, tools)
2. **Execute** - Create fresh agent, process request with loaded context
3. **Save** - Persist updated history back to Supabase
4. **Cleanup** - Destroy all runtime objects

Benefits:
- ✅ Survives server restarts
- ✅ Horizontally scalable
- ✅ No in-memory state
- ✅ Multi-tenant with Row-Level Security

### Database Schema

**sessions** - Session configuration and conversation history
- `session_id`, `client_id`, `engine_type`, `model`
- `conversation_history` (JSONB array)
- `tool_names` (TEXT array)
- `system_prompt`, timestamps

**clients** - Multi-tenant client accounts
- `client_id`, `display_name`
- `credentials_encrypted` (JSONB)
- Quotas, timestamps

**api_key_aliases** - API key management
- `key_hash` (SHA-256), `client_id`, `session_id`
- `key_type` ('base' or 'session')

---

## Troubleshooting

**"Authorization header required" (401)**
- Add `-H "Authorization: Bearer $API_KEY"` to your request
- Create client first with `POST /api/clients`

**"Invalid or expired API key" (401)**
- Verify correct API key
- Create new client if key was lost (cannot be retrieved)

**"Session not found" (404)**
- Session doesn't exist for your client (multi-tenant isolation)
- Verify you're using the correct client's API key

**"Database connection failed"**
- Check `SUPABASE_URL` and `SUPABASE_KEY` are set
- Verify migrations 001-006 are applied in order
- Check Supabase project is active

**Tools not working / Agent not using tools**
- Verify tools were registered: check session creation response includes `"tools": [...]`
- Ensure tool dependencies are set (e.g., `GITHUB_TOKEN` for github tool, `BRAVE_SEARCH_API_KEY` for web_search)
- Tools are only used when needed - the LLM decides when to invoke them
- Check server logs for verbose tool execution output (shows `[TOOL CALL]` messages)

---

## Self-Hosting

### Local Development

```bash
# Set environment variables
export SUPABASE_URL="https://your-project.supabase.co"
export SUPABASE_KEY="your-anon-key"
export OPENAI_API_KEY="your-openai-key"

# Build
mkdir build && cd build
cmake ..
make loom_api_stateless

# Run
./loom_api_stateless
```

### Docker

```bash
docker build -t loom-api .
docker run -p 8080:8080 \
  -e SUPABASE_URL="..." \
  -e SUPABASE_KEY="..." \
  -e OPENAI_API_KEY="..." \
  loom-api
```

### Database Setup

1. Create Supabase project
2. Apply migrations in order:
   ```bash
   # In Supabase SQL Editor
   # Run migrations/001_initial_schema.sql
   # Run migrations/002_simplify_client_id.sql
   # Run migrations/003_create_default_client.sql
   # Run migrations/004_enable_multi_tenancy.sql
   # Run migrations/005_create_rpc_function.sql
   # Run migrations/006_session_api_keys.sql
   ```

---

## Multi-Agent Workflows (Database Layer Only)

### Overview

The Loom API includes database-level support for agent-to-agent communication through **child sessions**. This allows one agent to delegate tasks to another specialized agent, with each agent maintaining its own conversation context.

**⚠️ IMPORTANT**: While the database infrastructure is implemented, HTTP API endpoints for this feature are **not currently exposed**. This section documents the underlying architecture for future implementation.

### Architecture

#### Child Sessions

When an agent needs to use another agent as a tool:

1. **Parent Session**: The main orchestrator agent
2. **Child Session**: A specialized agent registered as a tool to the parent
3. **Isolated Context**: Each child maintains its own conversation history
4. **Persistent Association**: Child sessions are linked to specific tool names in the parent

#### Database Methods

The `SupabaseClient` class provides these methods (see `include/SupabaseClient.hpp`):

```cpp
// Register a session as a tool for another session
bool registerChildSession(
    const std::string& parentSessionID,
    const std::string& toolName,
    const std::string& childSessionID,
    const std::string& clientID
);

// Retrieve the child session ID for a specific tool
std::optional<std::string> getChildSession(
    const std::string& parentSessionID,
    const std::string& toolName,
    const std::string& clientID
);

// Remove child session association
bool unregisterChildSession(
    const std::string& parentSessionID,
    const std::string& toolName,
    const std::string& clientID
);
```

### Use Cases

When implemented via API endpoints, this feature would enable:

1. **Task Specialization**
   - Main agent delegates research to a researcher agent with web tools
   - Main agent delegates calculations to a math specialist agent
   - Main agent delegates code generation to a programming agent

2. **Context Isolation**
   - Each child agent maintains its own conversation history
   - Parent only sees final results from child tool calls
   - Prevents context pollution between different task types

3. **Reusable Agents**
   - Create specialized agent sessions once
   - Register them as tools for multiple parent sessions
   - Each maintains independent state

### Example Architecture (Not Yet Implemented)

If API endpoints were added, the workflow would be:

```bash
# 1. Create a researcher agent with web tools
RESEARCHER_SESSION=$(curl -X POST $API_BASE_URL/api/sessions \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "tools": ["web_search", "web_fetch"]
  }' | jq -r '.session_id')

# 2. Create main orchestrator agent
MAIN_SESSION=$(curl -X POST $API_BASE_URL/api/sessions \
  -H "Authorization: Bearer $API_KEY" \
  -d '{"model": "gpt-4o-mini"}' | jq -r '.session_id')

# 3. Register researcher as a tool (ENDPOINT NOT IMPLEMENTED)
# This would call db_->registerChildSession() internally
curl -X POST $API_BASE_URL/api/sessions/$MAIN_SESSION/tools/agent \
  -H "Authorization: Bearer $API_KEY" \
  -d '{
    "source_session_id": "'$RESEARCHER_SESSION'",
    "tool_name": "researcher",
    "tool_description": "Agent that can search and read web content"
  }'

# 4. Main agent delegates to researcher (WOULD WORK AUTOMATICALLY)
curl -X POST $API_BASE_URL/v1/chat/completions \
  -H "Authorization: Bearer $SESSION_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{
      "role": "user",
      "content": "Use the researcher to find the latest AI safety papers"
    }]
  }'
```

### Database Schema

The `child_sessions` table tracks agent-to-agent relationships:

```sql
CREATE TABLE child_sessions (
    parent_session_id UUID REFERENCES sessions(session_id),
    tool_name TEXT,
    child_session_id UUID REFERENCES sessions(session_id),
    client_id UUID REFERENCES clients(client_id),
    created_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY (parent_session_id, tool_name)
);
```

### Implementation Status

- ✅ **Database layer**: Fully implemented in `SupabaseClient`
- ✅ **Schema**: `child_sessions` table exists
- ❌ **API endpoints**: Not exposed in `ApiServer_stateless.cpp`
- ❌ **Tool wrapper**: No AgentTool class to wrap child sessions

### Future Implementation

To complete this feature, the following would be needed:

1. **HTTP Endpoint**: `POST /api/sessions/{session_id}/tools/agent`
2. **AgentTool Class**: Wrapper that executes child session agent
3. **Tool Registry Integration**: Register child sessions like other tools
4. **Cleanup Logic**: Delete child sessions when parent is deleted

---

## License

MIT
