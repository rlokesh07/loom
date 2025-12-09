# Stateless Migration Plan for Loom API

---

## **Phase 1: Supabase Setup**

1. Create Supabase project
2. Get connection string (use the pooled connection URL)
3. Create database schema:
   - `clients` table (client_id, api_key_hash, credentials_encrypted, quotas)
   - `sessions` table (session_id, client_id, engine_type, model, conversation_history as JSONB, tool_names, timestamps)
   - `child_sessions` table (parent_session_id, tool_name, child_session_id)
   - `request_log` table (optional, for debugging/billing)
   - Add indexes on foreign keys and frequently queried columns
4. Apply schema using Supabase SQL editor or psql

---

## **Phase 2: Add Database Dependencies**

1. Install libpqxx (PostgreSQL C++ library)
2. Update CMakeLists.txt to include PostgreSQL and libpqxx
3. Add OpenSSL for credential encryption (optional, can do simple encryption first)

---

## **Phase 3: Create New Stateless Classes**

### **SessionRepository**
- Constructor takes database connection string
- Client operations: create_client, authenticate_client, get_client_credentials, set_client_credentials
- Session operations: create_session, get_session, verify_session_ownership, delete_session, list_client_sessions
- Conversation operations: get_conversation_history, append_messages, clear_history
- Tool operations: register_tools, get_session_tools, unregister_tool
- Child session operations: register_child_session, get_child_session

### **StatelessAgent**
- Takes CompletionEngine in constructor
- Main method: `execute(conversation_history, user_message, model, tools, temperature, max_tokens, verbose)`
- Returns: `ExecutionResult` struct containing response, new_messages to append, tool_calls, success/error
- Does NOT store any state between executions
- Works with conversation history passed as parameter

### **CredentialManager**
- Hash API keys for storage
- Encrypt/decrypt client credentials
- Initialize from environment variable (encryption key)

---

## **Phase 4: Rewrite API Server**

### **Update ApiServer class**
- Replace `AgentSessionManager` with `SessionRepository`
- Add `authenticate_request()` helper (checks Authorization header, calls SessionRepository)
- Add `create_engine()` helper (creates ChatGPT/Ollama engine with client credentials)
- Add `build_tool_registry()` helper (creates ToolRegistry from tool names)

### **Rewrite endpoints to be stateless**

**POST /api/clients**
- Parse api_key from request body
- Call SessionRepository.create_client()
- Return client_id

**POST /api/sessions**
- Authenticate request → get client_id
- Parse engine, model, system_prompt from body
- Call SessionRepository.create_session()
- Return session_id

**POST /api/sessions/:id/chat** (KEY ENDPOINT)
1. Authenticate request → get client_id
2. Verify session ownership
3. Load session template from database
4. Load conversation history from database
5. Load client credentials from database
6. Create completion engine with credentials
7. Build tool registry from session's tool names
8. Create StatelessAgent with engine
9. Execute agent with history + new message
10. Append result.new_messages to database
11. Return response with tool_calls

**GET /api/sessions/:id/history**
- Authenticate
- Verify ownership
- Load history from database
- Return messages

**DELETE /api/sessions/:id**
- Authenticate
- Verify ownership
- Delete from database

**GET /api/sessions**
- Authenticate
- List client's sessions from database

**POST /api/sessions/:id/tools**
- Authenticate
- Verify ownership
- Register tools in database

### **Update api_main.cpp**
- Initialize CredentialManager
- Create SessionRepository with DATABASE_URL from environment
- Pass SessionRepository to ApiServer
- Test database connection before starting server

---

## **Phase 5: Remove Old Stateful Code**

### **Delete entirely**
- `AgentSessionManager.hpp`
- `AgentSessionManager.cpp`

### **Modify Agent class** (or deprecate in favor of StatelessAgent)
- Remove `conversation_history_` member
- Remove `last_tool_calls_` member
- Remove `verbose_` member
- Remove `sessionID_` member
- Remove `session_manager_` pointer
- Keep as legacy or fully replace with StatelessAgent

---

## **Phase 6: Environment & Configuration**

### **Required environment variables**
- `DATABASE_URL` - Supabase connection string (pooled)
- `CREDENTIAL_ENCRYPTION_KEY` - 32-byte hex key for encrypting credentials
- `LOOM_API_PORT` - Port to run on (default 8080)

### **Remove old environment dependencies**
- No more global `OPENAI_API_KEY` (now per-client)
- No more global `GITHUB_TOKEN` (now per-client)
- No more global `BRAVE_SEARCH_API_KEY` (now per-client)

---

## **Phase 7: Testing**

1. Test database connection
2. Test client registration
3. Test session creation
4. Test sending messages (verify history persists across requests)
5. Test concurrent requests to same session from same client
6. Test multiple clients can't access each other's sessions
7. Test horizontal scaling (run 2 instances, verify both work)
8. Test database survives server restart (state persists)

---

## **Phase 8: Deploy Multiple Instances**

1. Build binary
2. Set environment variables
3. Run instance 1 on port 8080
4. Run instance 2 on port 8081
5. Put nginx or load balancer in front
6. Send requests to load balancer
7. Verify any instance can handle any request

---

## **What Gets Externalized (State → Database)**

| Current (In-Memory) | New (Database) |
|---------------------|----------------|
| `AgentSessionManager.sessions_` map | `sessions` table |
| `Agent.conversation_history_` vector | `conversation_history` JSONB column |
| `Agent.last_tool_calls_` vector | Returned in response, not stored |
| `AgentSession.child_sessions` map | `child_sessions` table |
| Global API keys from environment | Per-client encrypted credentials in `clients` table |

---

## **What Stays the Same**

- Tool system (ChatGPTTool, WebSearchTool, etc.)
- CompletionEngine interface (ChatGPT, Ollama)
- Tool calling mechanism
- Basic API structure (similar endpoints)

---

## **Critical Changes**

1. **Agent execution is now pure function**: `execute(history_in) → (response, new_messages_out)`
2. **Every request loads state from database, executes, saves state back**
3. **No in-memory state persists between requests**
4. **Authentication required on all endpoints** (Authorization: Bearer header)
5. **Sessions belong to clients** (multi-tenancy enforced)
6. **API keys and credentials stored encrypted in database**

---

## **Success Criteria**

- ✅ Server restarts don't lose conversations
- ✅ Two server instances can both serve same client
- ✅ Client A cannot access Client B's sessions
- ✅ Conversation history persists correctly across requests
- ✅ Tool calling still works
- ✅ No race conditions or data corruption under concurrent load
