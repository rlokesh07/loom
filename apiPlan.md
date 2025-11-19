# Loom Agent System REST API Implementation Plan

## Overview

This document outlines the plan to expose the Loom Agent system as a REST API for frontend integration. The API will support session management, chat interactions, tool registration, and multi-agent conversations.

---

## Architecture

### Technology Stack

- **HTTP Framework**: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only, lightweight)
- **JSON**: nlohmann/json (already in project)
- **Session Storage**: In-memory with optional persistence

> **Note**: Start with single-threaded implementation. Threading can be added later with minimal changes (see Phase 6).

### File Structure

```
loom/
├── include/
│   ├── ApiServer.hpp         # HTTP server class
│   ├── SessionManager.hpp    # Agent session management
│   ├── ConversationManager.hpp # Multi-agent conversation handling
│   └── ApiHandlers.hpp       # Request handlers
├── src/
│   ├── ApiServer.cpp
│   ├── SessionManager.cpp
│   ├── ConversationManager.cpp
│   ├── ApiHandlers.cpp
│   └── api_main.cpp          # API entry point
└── CMakeLists.txt            # Updated build config
```

---

## Implementation Phases

### Phase 1: Core Infrastructure

#### 1.1 Update CMakeLists.txt
- Add cpp-httplib dependency
- Create new executable target for API server
- Link existing loom library

#### 1.2 AgentSessionManager Class
```cpp
struct AgentSession {
    std::string id;
    std::shared_ptr<Agent> agent;
    std::string engine_type;  // "chatgpt" or "ollama"
    std::string model;
    std::chrono::time_point<std::chrono::system_clock> created_at;
};

class AgentSessionManager {
    std::map<std::string, AgentSession> sessions_;

public:
    std::string create_session(const std::string& engine_type,
                               const std::string& model,
                               bool verbose);
    AgentSession* get_session(const std::string& id);
    bool destroy_session(const std::string& id);
    std::vector<AgentSession> list_sessions();
};
```

#### 1.3 ApiServer Class
```cpp
class ApiServer {
    httplib::Server server_;
    AgentSessionManager session_manager_;
    ConversationManager conversation_manager_;

public:
    void configure_routes();
    void configure_cors();
    void start(int port);
    void stop();
};
```

---

### Phase 2: Core Endpoints

#### Session Management

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/sessions` | POST | Create new agent session |
| `/api/sessions` | GET | List all active sessions |
| `/api/sessions/{id}` | GET | Get session details |
| `/api/sessions/{id}` | DELETE | Destroy session |

**POST /api/sessions**
```json
// Request
{
  "engine": "chatgpt",        // "chatgpt" or "ollama"
  "model": "gpt-4o-mini",     // model name
  "verbose": false            // optional, default false
}

// Response
{
  "session_id": "sess-abc123",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z"
}
```

#### Chat Interaction

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/sessions/{id}/chat` | POST | Send message, get response |
| `/api/sessions/{id}/history` | GET | Get conversation history |
| `/api/sessions/{id}/history` | DELETE | Clear conversation history |

**POST /api/sessions/{id}/chat**
```json
// Request
{
  "message": "What is 2+2?",
  "system_prompt": "You are helpful.",  // optional, first message only
  "temperature": 0.7,                   // optional
  "max_tokens": 500                     // optional
}

// Response
{
  "success": true,
  "message": "2+2 equals 4.",
  "tool_calls": [],                     // tools that were called
  "usage": {
    "prompt_tokens": 25,
    "completion_tokens": 10
  }
}
```

**GET /api/sessions/{id}/history**
```json
// Response
{
  "session_id": "sess-abc123",
  "message_count": 4,
  "messages": [
    {"role": "system", "content": "You are helpful."},
    {"role": "user", "content": "What is 2+2?"},
    {"role": "assistant", "content": "2+2 equals 4."},
    {"role": "user", "content": "And 3+3?"}
  ]
}
```

#### Configuration

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/sessions/{id}/config` | GET | Get agent configuration |
| `/api/sessions/{id}/config` | PATCH | Update configuration |

**PATCH /api/sessions/{id}/config**
```json
// Request
{
  "verbose": true,
  "model": "gpt-4"
}

// Response
{
  "session_id": "sess-abc123",
  "verbose": true,
  "model": "gpt-4"
}
```

---

### Phase 3: Tool Management

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/tools/available` | GET | List all available tools |
| `/api/sessions/{id}/tools` | GET | List tools registered to session |
| `/api/sessions/{id}/tools` | POST | Register tools to session |
| `/api/sessions/{id}/tools/{name}` | DELETE | Unregister tool |

**GET /api/tools/available**
```json
// Response
{
  "tools": [
    {
      "name": "CalculatorTool",
      "description": "Performs basic arithmetic operations"
    },
    {
      "name": "WebSearchTool",
      "description": "Search the web using Brave Search API"
    },
    {
      "name": "WebFetchTool",
      "description": "Fetch and read web page content"
    },
    {
      "name": "GitHubTool",
      "description": "Interact with GitHub repositories"
    },
    {
      "name": "Base64Tool",
      "description": "Encode and decode Base64 strings"
    }
  ]
}
```

**POST /api/sessions/{id}/tools**
```json
// Request
{
  "tools": ["WebSearchTool", "WebFetchTool", "CalculatorTool"]
}

// Response
{
  "session_id": "sess-abc123",
  "registered_tools": ["WebSearchTool", "WebFetchTool", "CalculatorTool"],
  "tool_count": 3
}
```

---

### Phase 4: Multi-Agent Conversations

#### ConversationManager Class
```cpp
class ConversationManager {
    struct Conversation {
        std::string id;
        std::string agent1_id;
        std::string agent2_id;
        std::string status;  // "running", "completed", "error"
        std::vector<ConversationMessage> messages;
    };

    std::map<std::string, Conversation> conversations_;

public:
    // Runs conversation synchronously (blocking)
    std::string run_conversation(
        std::shared_ptr<Agent> agent1,
        std::shared_ptr<Agent> agent2,
        const json& agent1_config,
        const json& agent2_config
    );
    json get_conversation(const std::string& id);
};
```

#### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/conversations` | POST | Run multi-agent conversation (blocking) |
| `/api/conversations` | GET | List all completed conversations |
| `/api/conversations/{id}` | GET | Get conversation messages |

> **Note**: In single-threaded mode, POST `/api/conversations` blocks until conversation completes. Streaming and async execution added in Phase 6.

**POST /api/conversations**
```json
// Request
{
  "agent1_session_id": "sess-ollama123",
  "agent2_session_id": "sess-chatgpt456",
  "agent1_config": {
    "name": "Ollama Agent",
    "system_prompt": "You're fighting for equal rights for humans and robots. When you think you have won the debate, say COMPLETE. Use websearch tool for sources."
  },
  "agent2_config": {
    "name": "ChatGPT Agent",
    "system_prompt": "You are fighting to keep the status quo at all costs. When you think you have won the debate, say COMPLETE. Use websearch tool for sources."
  },
  "max_turns": 20  // optional, default 100
}

// Response (returns after conversation completes)
{
  "conversation_id": "conv-789",
  "status": "completed",
  "turns": 8,
  "agent1": "Ollama Agent",
  "agent2": "ChatGPT Agent",
  "messages": [...]
}
```

**GET /api/conversations/{id}**
```json
// Response
{
  "conversation_id": "conv-789",
  "status": "completed",  // "running", "completed", "error"
  "turns": 8,
  "messages": [
    {
      "turn": 1,
      "agent": "Ollama Agent",
      "content": "I believe robots deserve equal rights because...",
      "tool_calls": ["WebSearchTool"]
    },
    {
      "turn": 2,
      "agent": "ChatGPT Agent",
      "content": "The status quo exists for good reason...",
      "tool_calls": []
    }
  ],
  "completed_at": "2025-01-15T10:35:00Z"
}
```

---

### Phase 5: Advanced Features

#### 5.1 Session Persistence
```json
POST /api/sessions/{id}/save
// Saves session to disk

POST /api/sessions/{id}/load
// Loads session from disk
```

#### 5.2 Health & Metrics
```
GET /api/health
GET /api/metrics
```

---

### Phase 6: Threading & Concurrency (Future)

When you need to handle multiple concurrent requests:

#### 6.1 Enable Thread Pool
```cpp
// Single line change to enable threading
httplib::Server server;
server.new_task_queue = [] {
    return new httplib::ThreadPool(8);  // 8 worker threads
};
```

#### 6.2 Add Mutex Protection
Add `std::mutex` to SessionManager and ConversationManager to protect shared state:
```cpp
class SessionManager {
    std::map<std::string, std::shared_ptr<Agent>> sessions_;
    std::mutex mutex_;  // Add this

public:
    std::string create_session(...) {
        std::lock_guard<std::mutex> lock(mutex_);  // Lock before access
        // ... existing code
    }
};
```

#### 6.3 Async Conversations
Convert `run_conversation` to async with background threads:
- POST `/api/conversations` returns immediately with conversation ID
- Conversation runs in background thread
- Client polls GET `/api/conversations/{id}` for status
- Add SSE streaming endpoint `/api/conversations/{id}/stream`

#### 6.4 Per-Agent Mutex
Add mutex to Agent class to protect conversation history during concurrent access.

---

## Frontend Integration Example

### JavaScript Client

```javascript
class LoomClient {
  constructor(baseUrl = 'http://localhost:8080') {
    this.baseUrl = baseUrl;
  }

  // Create agent session
  async createSession(engine, model, verbose = false) {
    const response = await fetch(`${this.baseUrl}/api/sessions`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ engine, model, verbose })
    });
    return response.json();
  }

  // Send chat message
  async chat(sessionId, message, options = {}) {
    const response = await fetch(`${this.baseUrl}/api/sessions/${sessionId}/chat`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message, ...options })
    });
    return response.json();
  }

  // Register tools
  async registerTools(sessionId, tools) {
    const response = await fetch(`${this.baseUrl}/api/sessions/${sessionId}/tools`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ tools })
    });
    return response.json();
  }

  // Start multi-agent conversation
  async startConversation(agent1Id, agent2Id, agent1Config, agent2Config) {
    const response = await fetch(`${this.baseUrl}/api/conversations`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        agent1_session_id: agent1Id,
        agent2_session_id: agent2Id,
        agent1_config: agent1Config,
        agent2_config: agent2Config
      })
    });
    return response.json();
  }

  // Stream conversation with SSE
  streamConversation(conversationId, onMessage, onComplete) {
    const eventSource = new EventSource(
      `${this.baseUrl}/api/conversations/${conversationId}/stream`
    );

    eventSource.addEventListener('message', (e) => {
      onMessage(JSON.parse(e.data));
    });

    eventSource.addEventListener('complete', (e) => {
      onComplete(JSON.parse(e.data));
      eventSource.close();
    });

    return eventSource;
  }
}
```

### Usage Example: Ollama vs ChatGPT Debate

```javascript
async function runDebate() {
  const client = new LoomClient();

  // Create agents
  const ollama = await client.createSession('ollama', 'llama3.1:8b', true);
  const chatgpt = await client.createSession('chatgpt', 'gpt-4o-mini', true);

  // Register tools
  const tools = ['WebSearchTool', 'WebFetchTool'];
  await client.registerTools(ollama.session_id, tools);
  await client.registerTools(chatgpt.session_id, tools);

  // Start conversation
  const conv = await client.startConversation(
    ollama.session_id,
    chatgpt.session_id,
    {
      name: 'Ollama Agent',
      system_prompt: 'You fight for robot rights. Say COMPLETE when done.'
    },
    {
      name: 'ChatGPT Agent',
      system_prompt: 'You defend the status quo. Say COMPLETE when done.'
    }
  );

  // Stream updates to UI
  client.streamConversation(
    conv.conversation_id,
    (msg) => {
      console.log(`${msg.agent}: ${msg.content}`);
      // Update UI with new message
    },
    (result) => {
      console.log(`Conversation complete after ${result.total_turns} turns`);
    }
  );
}
```

---

## Technical Considerations

### Timeout Handling
- LLM calls can take 10-30 seconds
- Set appropriate request timeouts in cpp-httplib
- Consider client-side timeout handling

### Error Handling
```json
// Error response format
{
  "error": {
    "code": "SESSION_NOT_FOUND",
    "message": "Session with ID 'sess-xyz' not found"
  }
}
```

Error codes:
- `SESSION_NOT_FOUND`
- `CONVERSATION_NOT_FOUND`
- `TOOL_NOT_FOUND`
- `INVALID_ENGINE`
- `API_KEY_MISSING`
- `OLLAMA_NOT_RUNNING`

### CORS Configuration
```cpp
server.set_default_headers({
    {"Access-Control-Allow-Origin", "*"},
    {"Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS"},
    {"Access-Control-Allow-Headers", "Content-Type"}
});
```

### Memory Management
- Manual session cleanup (call DELETE when done)
- Consider adding session TTL cleanup in Phase 6
- Limit conversation history size if needed

---

## Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `OPENAI_API_KEY` | For ChatGPT | OpenAI API authentication |
| `BRAVE_SEARCH_API_KEY` | For WebSearchTool | Brave Search API key |
| `GITHUB_TOKEN` | For GitHubTool | GitHub API token |
| `LOOM_API_PORT` | Optional | Server port (default: 8080) |
| `LOOM_MAX_SESSIONS` | Optional | Max concurrent sessions |

---

## Build & Run

```bash
# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run API server
./loom_api

# Server starts on http://localhost:8080
```
