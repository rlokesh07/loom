# Plan: Replace Chat Endpoints with OpenAI-Compatible Format

## Executive Summary

Replace Loom's current chat completion endpoints with OpenAI-compatible format using session-specific API keys. Since there are **no existing users**, we can make breaking changes for maximum simplicity and cleanliness.

---

## Architecture

### Session-Specific API Keys

```python
from openai import OpenAI

# 1. Create session, get session-specific key
session = create_session(base_key, model="gpt-4")
session_key = session["session_api_key"]  # e.g., "sk-session-abc123..."

# 2. Use with OpenAI SDK - zero modifications needed
client = OpenAI(
    base_url="http://localhost:8080/v1",
    api_key=session_key  # Session automatically selected!
)

# 3. Chat completions work exactly like OpenAI
response = client.chat.completions.create(
    model="gpt-4",
    messages=[{"role": "user", "content": "Hello"}]
)
```

### Request Flow

```
API Key → Hash → Lookup in api_key_aliases (single source of truth)
    ↓
  Get client_id + session_id (if session key)
    ↓
  Load session history (if session key)
    ↓
  Execute agent
    ↓
  Return OpenAI-format response
```

---

## Endpoints

### Chat Completions

**REMOVE:**
- ❌ `POST /api/sessions/{session_id}/chat` (old format)

**ADD:**
- ✅ `POST /v1/chat/completions` (OpenAI-compatible)

**Request (OpenAI format):**
```json
{
  "model": "gpt-4",
  "messages": [
    {"role": "system", "content": "You are helpful"},
    {"role": "user", "content": "Hello"}
  ],
  "temperature": 0.7,
  "max_tokens": 100
}
```

**Response (OpenAI format):**
```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1677652288,
  "model": "gpt-4",
  "choices": [{
    "index": 0,
    "message": {
      "role": "assistant",
      "content": "Hello! How can I help?"
    },
    "finish_reason": "stop"
  }],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 9,
    "total_tokens": 19
  }
}
```

### Session Management (Keep)

Keep these endpoints:
- ✅ `POST /api/sessions` - Create session, returns `session_api_key`
- ✅ `GET /api/sessions` - List sessions
- ✅ `GET /api/sessions/{id}` - Get session details
- ✅ `DELETE /api/sessions/{id}` - Delete session (cascades to key)
- ✅ `GET /api/sessions/{id}/history` - Get conversation history
- ✅ `DELETE /api/sessions/{id}/history` - Clear history
- ✅ Tool management endpoints (unchanged)

---

## Database Changes

### Migration: 006_session_api_keys.sql

**File:** `migrations/006_session_api_keys.sql`

```sql
-- ============================================
-- Session-Specific API Keys Migration
-- ============================================

-- Create api_key_aliases table (single source of truth for all keys)
CREATE TABLE IF NOT EXISTS api_key_aliases (
    id SERIAL PRIMARY KEY,
    key_hash TEXT NOT NULL UNIQUE,
    client_id UUID NOT NULL REFERENCES clients(client_id) ON DELETE CASCADE,
    session_id UUID REFERENCES sessions(session_id) ON DELETE CASCADE,
    key_type TEXT NOT NULL CHECK (key_type IN ('base', 'session')),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    last_used_at TIMESTAMPTZ,
    is_active BOOLEAN DEFAULT true
);

CREATE INDEX idx_key_aliases_hash ON api_key_aliases(key_hash);
CREATE INDEX idx_key_aliases_client ON api_key_aliases(client_id);
CREATE INDEX idx_key_aliases_session ON api_key_aliases(session_id);

-- Row-level security
ALTER TABLE api_key_aliases ENABLE ROW LEVEL SECURITY;

CREATE POLICY api_key_aliases_isolation ON api_key_aliases
    FOR ALL
    USING (client_id = current_setting('app.current_client_id', true)::uuid);

-- ============================================
-- Clean up clients table
-- ============================================

-- Remove api_key_hash column from clients table
-- (No longer needed - all keys stored in api_key_aliases)
ALTER TABLE clients DROP COLUMN IF EXISTS api_key_hash;

-- Note: No data migration needed since there are no existing users
```

**Rollback (if needed):**
```sql
-- Rollback migration
DROP POLICY IF EXISTS api_key_aliases_isolation ON api_key_aliases;
DROP TABLE IF EXISTS api_key_aliases;

-- Restore clients table structure
ALTER TABLE clients ADD COLUMN IF NOT EXISTS api_key_hash TEXT;
CREATE UNIQUE INDEX IF NOT EXISTS idx_clients_api_key ON clients(api_key_hash);
```

---

## Code Implementation

### 1. Update SupabaseClient.hpp

**Delete old authentication method:**
```cpp
// DELETE THIS (lines 88-91):
std::optional<std::string> authenticate_client(const std::string& api_key_hash);
```

**Add new methods after line 100:**

```cpp
    // ===== Session-Specific API Key Methods =====

    struct ApiKeyAlias {
        std::string key_hash;
        std::string client_id;
        std::optional<std::string> session_id;
        std::string key_type;  // "base" or "session"
        bool is_active;
    };

    // Authenticate API key (base or session) - ONLY authentication method
    std::optional<ApiKeyAlias> authenticateApiKey(
        const std::string& key_hash
    );

    // Create base API key for a client
    std::optional<std::string> createBaseApiKey(
        const std::string& client_id
    );

    // Create session-specific API key (returns unhashed key)
    std::optional<std::string> createSessionApiKey(
        const std::string& client_id,
        const std::string& session_id
    );

    // List all API keys for a client
    std::vector<ApiKeyAlias> listClientApiKeys(
        const std::string& client_id
    );

    // Revoke an API key
    bool revokeApiKey(
        const std::string& key_hash,
        const std::string& client_id
    );
```

**Update create_client() signature (line 95):**
```cpp
// CHANGE FROM:
std::optional<std::string> create_client(const std::string& api_key_hash,
                                          const std::string& display_name);

// CHANGE TO:
std::optional<std::string> create_client(const std::string& display_name);
```

### 2. Update SupabaseClient.cpp

**Delete old authenticate_client() method:**
```cpp
// DELETE lines 464-482 (entire authenticate_client method)
```

**Update create_client() method:**

Replace existing implementation (lines 484-506) with:

```cpp
std::optional<std::string> SupabaseClient::create_client(
    const std::string& display_name
) {
    try {
        // Create client WITHOUT api_key_hash
        json body = {
            {"display_name", display_name}
        };

        std::string response = makeRequest("POST", "/rest/v1/clients", body);
        json response_json = json::parse(response);

        if (response_json.is_array() && !response_json.empty()) {
            return response_json[0]["client_id"].get<std::string>();
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error creating client: " << e.what() << std::endl;
        return std::nullopt;
    }
}
```

**Add new API key methods at end of file (before closing):**

```cpp
// ============================================
// Session-Specific API Key Methods
// ============================================

std::optional<SupabaseClient::ApiKeyAlias> SupabaseClient::authenticateApiKey(
    const std::string& key_hash
) {
    try {
        std::string query = "key_hash=eq." + key_hash + "&is_active=eq.true&select=*";
        std::string response = makeRequest("GET", "/rest/v1/api_key_aliases",
                                          json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                json row = response_json[0];

                ApiKeyAlias alias;
                alias.key_hash = row["key_hash"];
                alias.client_id = row["client_id"];
                alias.key_type = row["key_type"];
                alias.is_active = row["is_active"];

                if (!row["session_id"].is_null()) {
                    alias.session_id = row["session_id"];
                }

                return alias;
            }
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error authenticating API key: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<std::string> SupabaseClient::createBaseApiKey(
    const std::string& client_id
) {
    try {
        // Import AuthManager for key generation/hashing
        // (Reuse existing crypto code - don't duplicate!)
        extern std::string generate_api_key_impl();
        extern std::string hash_api_key_impl(const std::string&);

        // Generate base key
        std::string base_key = generate_api_key_impl();
        std::string key_hash = hash_api_key_impl(base_key);

        json body = {
            {"key_hash", key_hash},
            {"client_id", client_id},
            {"key_type", "base"}
        };

        std::string response = makeRequest("POST", "/rest/v1/api_key_aliases", body);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                return base_key;  // Return unhashed key
            }
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error creating base API key: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<std::string> SupabaseClient::createSessionApiKey(
    const std::string& client_id,
    const std::string& session_id
) {
    try {
        extern std::string generate_api_key_impl();
        extern std::string hash_api_key_impl(const std::string&);

        // Generate session key with distinctive prefix
        std::string random_part = generate_api_key_impl().substr(3);  // Remove "sk-"
        std::string session_key = "sk-session-" + random_part;
        std::string key_hash = hash_api_key_impl(session_key);

        json body = {
            {"key_hash", key_hash},
            {"client_id", client_id},
            {"session_id", session_id},
            {"key_type", "session"}
        };

        std::string response = makeRequest("POST", "/rest/v1/api_key_aliases", body);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                return session_key;  // Return unhashed key
            }
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error creating session API key: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<SupabaseClient::ApiKeyAlias> SupabaseClient::listClientApiKeys(
    const std::string& client_id
) {
    std::vector<ApiKeyAlias> keys;

    try {
        std::string query = "client_id=eq." + client_id + "&select=*&order=created_at.desc";
        std::string response = makeRequest("GET", "/rest/v1/api_key_aliases",
                                          json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array()) {
                for (const auto& row : response_json) {
                    ApiKeyAlias alias;
                    alias.key_hash = row["key_hash"];
                    alias.client_id = row["client_id"];
                    alias.key_type = row["key_type"];
                    alias.is_active = row["is_active"];

                    if (!row["session_id"].is_null()) {
                        alias.session_id = row["session_id"];
                    }

                    keys.push_back(alias);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing API keys: " << e.what() << std::endl;
    }

    return keys;
}

bool SupabaseClient::revokeApiKey(
    const std::string& key_hash,
    const std::string& client_id
) {
    try {
        json update_body = {{"is_active", false}};
        std::string query = "key_hash=eq." + key_hash + "&client_id=eq." + client_id;

        makeRequest("PATCH", "/rest/v1/api_key_aliases", update_body, query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error revoking API key: " << e.what() << std::endl;
        return false;
    }
}
```

**Note:** The `extern` declarations are temporary. You'll need to either:
- Make AuthManager methods static/accessible
- Pass AuthManager to SupabaseClient constructor
- Or include `AuthManager.hpp` and call methods directly

### 3. Update AuthManager (Make Methods Accessible)

**Option A: Make methods static (recommended)**

**File:** `include/AuthManager.hpp`

```cpp
class AuthManager {
public:
    // Make these static so SupabaseClient can use them
    static std::string generate_api_key();
    static std::string hash_api_key(const std::string& api_key);

    // Keep existing authenticate method (uses static hash_api_key internally)
    std::optional<std::string> authenticate(
        const std::string& api_key,
        std::shared_ptr<SupabaseClient> db
    );
};
```

**File:** `src/AuthManager.cpp`

```cpp
// Update methods to be static
std::string AuthManager::generate_api_key() {  // Remove 'static' from implementation
    // ... existing implementation ...
}

std::string AuthManager::hash_api_key(const std::string& api_key) {
    // ... existing implementation ...
}
```

**Then in SupabaseClient.cpp:**

```cpp
#include "../include/AuthManager.hpp"

// Replace extern declarations with:
std::string base_key = AuthManager::generate_api_key();
std::string key_hash = AuthManager::hash_api_key(base_key);
```

### 4. Update ApiServer Client Creation

**File:** `src/ApiServer_stateless.cpp`

Update `handle_create_client()`:

```cpp
void ApiServer::handle_create_client(
    const httplib::Request& req,
    httplib::Response& res
) {
    // No authentication needed - public endpoint

    json request_body = json::parse(req.body);
    std::string display_name = request_body.value("display_name", "");

    // Create client (no API key yet)
    auto client_id_opt = supabase_->create_client(display_name);
    if (!client_id_opt) {
        return send_error(res, 500, "INTERNAL_ERROR", "Failed to create client");
    }

    std::string client_id = *client_id_opt;

    // Generate base API key
    auto base_key_opt = supabase_->createBaseApiKey(client_id);
    if (!base_key_opt) {
        // Client created but key generation failed - orphaned client
        // Could delete client here or just error
        return send_error(res, 500, "INTERNAL_ERROR",
                         "Failed to generate API key");
    }

    json response = {
        {"client_id", client_id},
        {"api_key", *base_key_opt},  // Return unhashed key (only time shown!)
        {"message", "Save this API key - it won't be shown again"}
    };

    res.set_content(response.dump(), "application/json");
    res.status = 201;
}
```

### 5. Update ApiServer Authentication

**File:** `src/ApiServer_stateless.cpp`

Update authentication method:

```cpp
bool ApiServer::authenticate_request(
    const httplib::Request& req,
    httplib::Response& res,
    std::string& client_id_out,
    std::optional<std::string>& session_id_out
) {
    if (!req.has_header("Authorization")) {
        send_error(res, 401, "MISSING_AUTH", "Missing Authorization header");
        return false;
    }

    std::string auth_header = req.get_header_value("Authorization");
    if (auth_header.find("Bearer ") != 0) {
        send_error(res, 401, "INVALID_AUTH_FORMAT",
                   "Authorization must be 'Bearer {api_key}'");
        return false;
    }

    std::string api_key = auth_header.substr(7);
    std::string key_hash = AuthManager::hash_api_key(api_key);  // Use static method

    // Look up in api_key_aliases table (only authentication method)
    auto alias_opt = supabase_->authenticateApiKey(key_hash);
    if (!alias_opt) {
        send_error(res, 401, "INVALID_API_KEY", "Invalid or inactive API key");
        return false;
    }

    auto alias = *alias_opt;
    client_id_out = alias.client_id;
    session_id_out = alias.session_id;

    supabase_->set_client_context(client_id_out);
    return true;
}
```

Update REQUIRE_AUTH macro:
```cpp
#define REQUIRE_AUTH() \
    std::string client_id; \
    std::optional<std::string> session_id; \
    if (!authenticate_request(req, res, client_id, session_id)) return;
```

### 6. Update Session Creation

**File:** `src/ApiServer_stateless.cpp`

Update `handle_create_session()`:

```cpp
void ApiServer::handle_create_session(
    const httplib::Request& req,
    httplib::Response& res
) {
    REQUIRE_AUTH();  // Must use base key (session key can't create sessions)

    // Validate that they're using a base key, not a session key
    if (session_id.has_value()) {
        return send_error(res, 400, "INVALID_REQUEST",
                         "Cannot create session with session key. Use base key.");
    }

    json request_body = json::parse(req.body);

    std::string engine = request_body["engine"];
    std::string model = request_body["model"];
    std::string system_prompt = request_body.value("system_prompt", "");

    // Create session
    auto new_session_id_opt = supabase_->createSession(engine, model, client_id, system_prompt);
    if (!new_session_id_opt) {
        return send_error(res, 500, "INTERNAL_ERROR", "Failed to create session");
    }

    std::string new_session_id = *new_session_id_opt;

    // Generate session-specific API key
    auto session_key_opt = supabase_->createSessionApiKey(client_id, new_session_id);
    if (!session_key_opt) {
        // Session created but key generation failed
        // Delete the session to avoid orphans
        supabase_->deleteSession(new_session_id, client_id);
        return send_error(res, 500, "INTERNAL_ERROR",
                         "Failed to generate session API key");
    }

    json response = {
        {"session_id", new_session_id},
        {"session_api_key", *session_key_opt},  // Return unhashed key (only time shown!)
        {"engine", engine},
        {"model", model},
        {"system_prompt", system_prompt},
        {"created_at", get_current_timestamp()}
    };

    res.set_content(response.dump(), "application/json");
    res.status = 201;
}
```

### 7. Replace Chat Endpoint with OpenAI Format

**REMOVE old endpoint:**
```cpp
// DELETE THIS:
svr_.Post("/api/sessions/:session_id/chat", [this](...) { ... });
```

**ADD OpenAI endpoint:**

```cpp
void ApiServer::setup_routes() {
    // ... existing routes ...

    // OpenAI-compatible endpoints
    svr_.Post("/v1/chat/completions",
        [this](const httplib::Request& req, httplib::Response& res) {
            handle_chat_completion(req, res);
        }
    );

    svr_.Get("/v1/models",
        [this](const httplib::Request& req, httplib::Response& res) {
            handle_list_models(req, res);
        }
    );
}

void ApiServer::handle_chat_completion(
    const httplib::Request& req,
    httplib::Response& res
) {
    // Authenticate
    std::string client_id;
    std::optional<std::string> session_id;
    if (!authenticate_request(req, res, client_id, session_id)) {
        return;
    }

    // Parse request
    json request_body;
    try {
        request_body = json::parse(req.body);
    } catch (const json::exception& e) {
        return send_error(res, 400, "INVALID_REQUEST", "Invalid JSON");
    }

    if (!request_body.contains("model") || !request_body.contains("messages")) {
        return send_error(res, 400, "INVALID_REQUEST",
                         "Missing required fields: model, messages");
    }

    // Route based on key type
    if (session_id.has_value()) {
        // Session key: use session history
        handle_session_completion(res, client_id, *session_id, request_body);
    } else {
        // Base key: stateless
        handle_stateless_completion(res, client_id, request_body);
    }
}

void ApiServer::handle_session_completion(
    httplib::Response& res,
    const std::string& client_id,
    const std::string& session_id,
    const json& request_body
) {
    // Load session
    auto session_opt = supabase_->getSession(session_id, client_id);
    if (!session_opt) {
        return send_error(res, 404, "SESSION_NOT_FOUND", "Session not found");
    }
    auto session = *session_opt;

    // Load history
    auto history = supabase_->getConversationHistory(session_id, client_id);

    // Create agent
    auto engine = create_engine(session.engine_type);
    Agent agent(engine, session.model, false);

    // Load existing messages
    for (const auto& msg : history) {
        agent.add_message(msg.role, msg.content);
    }

    // IMPROVED: Smart deduplication - only check tail of history
    size_t history_size = history.size();
    auto request_messages = request_body["messages"];
    size_t request_size = request_messages.size();

    std::vector<ChatMessage> new_messages;

    // If request has more messages than history, new ones are at the end
    if (request_size > history_size) {
        for (size_t i = history_size; i < request_size; i++) {
            std::string role = request_messages[i]["role"];
            std::string content = request_messages[i]["content"];

            agent.add_message(role, content);
            new_messages.push_back(ChatMessage(role, content));
        }
    }
    // else: request messages == history (no new messages, maybe retry)

    // Execute
    AgentExecutionParams params;
    params.temperature = request_body.value("temperature", 1.0);
    params.max_tokens = request_body.value("max_tokens", -1);

    auto result = agent.execute_safe(params);

    // Save new messages
    for (const auto& msg : new_messages) {
        supabase_->add_message(session_id, client_id, msg.role, msg.content);
    }

    // Save assistant response
    if (result.success) {
        supabase_->add_message(session_id, client_id, "assistant", result.message);
    }

    // Build OpenAI response
    json openai_response = build_openai_response(result, session.model);

    res.set_content(openai_response.dump(), "application/json");
    res.status = 200;
}

void ApiServer::handle_stateless_completion(
    httplib::Response& res,
    const std::string& client_id,
    const json& request_body
) {
    std::string model = request_body["model"];

    // IMPROVED: Validate model and infer engine
    std::string engine_type;
    try {
        engine_type = infer_engine_from_model(model);
    } catch (const std::invalid_argument& e) {
        return send_error(res, 400, "INVALID_MODEL", e.what());
    }

    // Create agent
    auto engine = create_engine(engine_type);
    Agent agent(engine, model, false);

    // Load all messages
    for (const auto& msg : request_body["messages"]) {
        agent.add_message(msg["role"], msg["content"]);
    }

    // Execute
    AgentExecutionParams params;
    params.temperature = request_body.value("temperature", 1.0);
    params.max_tokens = request_body.value("max_tokens", -1);

    auto result = agent.execute_safe(params);

    // Build OpenAI response
    json openai_response = build_openai_response(result, model);

    res.set_content(openai_response.dump(), "application/json");
    res.status = 200;
}

// Exact model lookup table
const std::unordered_map<std::string, std::string> ApiServer::MODEL_ENGINE_MAP = {
    // OpenAI models
    {"gpt-4", "chatgpt"},
    {"gpt-4-turbo", "chatgpt"},
    {"gpt-4-turbo-preview", "chatgpt"},
    {"gpt-4o", "chatgpt"},
    {"gpt-4o-mini", "chatgpt"},
    {"gpt-3.5-turbo", "chatgpt"},
    {"gpt-3.5-turbo-16k", "chatgpt"},
    {"o1-preview", "chatgpt"},
    {"o1-mini", "chatgpt"},

    // Gemini models
    {"gemini-pro", "gemini"},
    {"gemini-1.5-pro", "gemini"},
    {"gemini-1.5-flash", "gemini"},
    {"gemini-ultra", "gemini"},

    // Ollama models (common ones)
    {"llama3", "ollama"},
    {"llama3:8b", "ollama"},
    {"llama3:70b", "ollama"},
    {"llama2", "ollama"},
    {"llama2:7b", "ollama"},
    {"llama2:13b", "ollama"},
    {"mistral", "ollama"},
    {"mistral:7b", "ollama"},
    {"mixtral", "ollama"},
    {"mixtral:8x7b", "ollama"},
    {"phi", "ollama"},
    {"phi:2.7b", "ollama"},
    {"codellama", "ollama"},
    {"codellama:7b", "ollama"},
    {"codellama:13b", "ollama"},
    {"qwen", "ollama"},
    {"qwen:7b", "ollama"}
};

std::string ApiServer::infer_engine_from_model(const std::string& model) {
    // Exact match lookup
    auto it = MODEL_ENGINE_MAP.find(model);

    if (it != MODEL_ENGINE_MAP.end()) {
        return it->second;
    }

    // Model not found - return helpful error
    throw std::invalid_argument(
        "Unknown model: '" + model + "'. " +
        "Supported models: gpt-4, gpt-4-turbo, gpt-3.5-turbo, o1-preview, " +
        "gemini-pro, gemini-1.5-pro, llama3, mistral, phi, codellama, etc. " +
        "See /v1/models endpoint for full list."
    );
}

// Helper: Build OpenAI response
json ApiServer::build_openai_response(
    const AgentResult& result,
    const std::string& model
) {
    return {
        {"id", "chatcmpl-" + generate_random_id()},
        {"object", "chat.completion"},
        {"created", std::time(nullptr)},
        {"model", model},
        {"choices", json::array({
            {
                {"index", 0},
                {"message", {
                    {"role", "assistant"},
                    {"content", result.message}
                }},
                {"finish_reason", result.success ? "stop" : "error"}
            }
        })},
        {"usage", {
            {"prompt_tokens", 0},      // TODO: Extract from engine if available
            {"completion_tokens", 0},
            {"total_tokens", 0}
        }}
    };
}

std::string ApiServer::generate_random_id() {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);

    std::string id;
    for (int i = 0; i < 29; i++) {
        id += chars[dis(gen)];
    }
    return id;
}

// Helper: List all supported models
void ApiServer::handle_list_models(
    const httplib::Request& req,
    httplib::Response& res
) {
    // Build list of all supported models from the map
    json models_array = json::array();

    for (const auto& [model_name, engine] : MODEL_ENGINE_MAP) {
        models_array.push_back({
            {"id", model_name},
            {"object", "model"},
            {"created", 1677649963},  // Static timestamp
            {"owned_by", engine}      // Use engine as owner
        });
    }

    json response = {
        {"object", "list"},
        {"data", models_array}
    };

    res.set_content(response.dump(), "application/json");
    res.status = 200;
}
```

---

## Implementation Checklist

### Database
- [ ] Create `migrations/006_session_api_keys.sql`
- [ ] Remove `api_key_hash` from `clients` table
- [ ] Create `api_key_aliases` table with RLS

### AuthManager
- [ ] Make `generate_api_key()` static
- [ ] Make `hash_api_key()` static
- [ ] Update `authenticate()` to use static methods

### SupabaseClient
- [ ] **DELETE** `authenticate_client()` method (hpp + cpp)
- [ ] Update `create_client()` signature (remove api_key_hash param)
- [ ] Add `ApiKeyAlias` struct to .hpp
- [ ] Add new method declarations to .hpp
- [ ] Implement `authenticateApiKey()`
- [ ] Implement `createBaseApiKey()`
- [ ] Implement `createSessionApiKey()`
- [ ] Implement `listClientApiKeys()`
- [ ] Implement `revokeApiKey()`

### ApiServer
- [ ] Update `handle_create_client()` to use new flow
- [ ] Update `authenticate_request()` signature and implementation
- [ ] Update `REQUIRE_AUTH` macro
- [ ] Update `handle_create_session()` to:
  - Validate base key usage
  - Generate session key
  - Rollback on key generation failure
- [ ] **DELETE** old `/api/sessions/{id}/chat` endpoint
- [ ] **ADD** `/v1/chat/completions` endpoint
- [ ] Implement `handle_chat_completion()`
- [ ] Implement `handle_session_completion()` with improved deduplication
- [ ] Implement `handle_stateless_completion()`
- [ ] Implement `infer_engine_from_model()` with validation
- [ ] Implement `build_openai_response()` helper
- [ ] Implement `generate_random_id()` helper

### Testing
- [ ] Test client creation returns base key
- [ ] Test session creation with base key
- [ ] Test session creation with session key (should fail)
- [ ] Test authentication with session key
- [ ] Test authentication with base key
- [ ] Test `/v1/chat/completions` with session key
- [ ] Test `/v1/chat/completions` with base key (stateless)
- [ ] Test unknown model names (should error)
- [ ] Test with OpenAI Python SDK
- [ ] Test session deletion cascades to key
- [ ] Test orphaned session rollback

### Documentation
- [ ] Update API_USAGE.md with new format
- [ ] Remove old endpoint docs
- [ ] Add OpenAI SDK examples
- [ ] Document session key vs base key
- [ ] Document stateless mode requirements
- [ ] Note streaming is not supported

---

## Usage Examples

### Create Client and Get Base Key

```bash
curl -X POST http://localhost:8080/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name": "My App"}'

# Response:
{
  "client_id": "uuid-abc",
  "api_key": "sk-aBcD1234...",
  "message": "Save this API key - it won't be shown again"
}
```

### Create Session and Get Session Key

```bash
curl -X POST http://localhost:8080/api/sessions \
  -H "Authorization: Bearer sk-aBcD1234..." \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "chatgpt",
    "model": "gpt-4"
  }'

# Response:
{
  "session_id": "uuid-123",
  "session_api_key": "sk-session-xYz789...",
  "engine": "chatgpt",
  "model": "gpt-4",
  "created_at": "2024-01-01T00:00:00Z"
}
```

### Use with OpenAI SDK (Python)

```python
from openai import OpenAI

# Use session key
client = OpenAI(
    base_url="http://localhost:8080/v1",
    api_key="sk-session-xYz789..."
)

response = client.chat.completions.create(
    model="gpt-4",
    messages=[{"role": "user", "content": "Hello"}]
)

print(response.choices[0].message.content)
```

### Stateless Mode (Base Key)

```python
# Use base key for stateless requests
client = OpenAI(
    base_url="http://localhost:8080/v1",
    api_key="sk-aBcD1234..."  # Base key
)

# Must send full conversation history each time
response = client.chat.completions.create(
    model="gpt-4",
    messages=[
        {"role": "system", "content": "You are helpful"},
        {"role": "user", "content": "Hello"},
        {"role": "assistant", "content": "Hi there!"},
        {"role": "user", "content": "How are you?"}
    ]
)
```

---

## Key Improvements Over Original Plan

1. ✅ **Removed `authenticate_client()`** - single auth method (authenticateApiKey)
2. ✅ **Removed `api_key_hash` from clients** - cleaner schema
3. ✅ **No data migration needed** - no existing users
4. ✅ **Reuse AuthManager crypto** - no code duplication
5. ✅ **Improved deduplication** - O(1) instead of O(n×m)
6. ✅ **Exact model lookup** - whitelist of supported models, no pattern matching
7. ✅ **Session key prefix** - `sk-session-` instead of `sk-sess-`
8. ✅ **Orphan prevention** - rollback session if key generation fails
9. ✅ **Base key enforcement** - can't create sessions with session keys
10. ✅ **Better error messages** - OpenAI-style validation
11. ✅ **List models endpoint** - `/v1/models` returns all supported models

---

## Summary

**What Changes:**
- ❌ Remove `/api/sessions/{id}/chat` endpoint
- ❌ Remove `authenticate_client()` method
- ❌ Remove `api_key_hash` from clients table
- ✅ Add `api_key_aliases` table (single source of truth)
- ✅ Add `/v1/chat/completions` (OpenAI format)
- ✅ Session creation returns `session_api_key`
- ✅ Works with OpenAI SDK out of the box

**What Stays:**
- ✅ All session management endpoints
- ✅ History endpoints
- ✅ Tool endpoints
- ✅ Child session (agent-as-tool) architecture

**Benefits:**
- Industry standard API format
- Drop-in replacement for OpenAI
- Cleaner codebase (no legacy code)
- No backward compatibility burden
- Single authentication system
