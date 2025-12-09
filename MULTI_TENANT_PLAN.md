# Multi-Tenant Deployment Plan

## Overview
Convert the current single-tenant Loom API to full multi-tenant deployment with API key authentication and client isolation.

---

## Current State Problems

1. **No Authentication**: All requests use a hardcoded `default_client_id`
2. **No Client Management**: Can't create or manage multiple clients
3. **Simplified Schema**: Migration 002 removed UUID foreign keys and RLS for "easier testing"
4. **Single Tenant**: Server constructor takes one `client_id` for all requests

---

## Required Changes

### Phase 1: Database Schema (Migration)

**File**: `migrations/004_enable_multi_tenancy.sql`

Changes:
- ✅ Restore `client_id` as UUID with foreign key to `clients` table
- ✅ Re-enable Row Level Security (RLS) on `sessions` and `child_sessions`
- ✅ Create RLS policies that use `app.current_client_id` session variable
- ✅ Add index on `clients.api_key_hash` for fast lookups

**Why**: Database must enforce tenant isolation at the data layer

---

### Phase 2: Authentication System

**New Files**:
- `include/AuthManager.hpp`
- `src/AuthManager.cpp`

**Functionality**:
```cpp
class AuthManager {
    // Create new client, return (client_id, api_key)
    std::optional<pair<string, string>> create_client(display_name);

    // Validate API key, return client_id
    std::optional<string> authenticate(api_key);

    // Extract "Bearer sk-xxx" from header
    static std::optional<string> extract_api_key(auth_header);

private:
    // SHA-256 hash for storing API keys
    string hash_api_key(api_key);

    // Generate random API key (sk-32_random_chars)
    string generate_api_key();
};
```

**Dependencies**: OpenSSL for SHA-256 hashing

---

### Phase 3: Update SupabaseClient

**File**: `src/SupabaseClient.cpp`

**New Method**:
```cpp
// Authenticate API key and return client_id
std::optional<string> SupabaseClient::authenticate_client(const string& api_key_hash) {
    // Query: SELECT client_id FROM clients WHERE api_key_hash = $1
    // Return client_id if found
}

// Create new client
std::optional<string> SupabaseClient::create_client(
    const string& api_key_hash,
    const string& display_name
) {
    // INSERT INTO clients (api_key_hash, display_name)
    // RETURNING client_id
}

// Set RLS session variable before queries
void SupabaseClient::set_client_context(const string& client_id) {
    // Execute: SET LOCAL app.current_client_id = 'client_id'
    // This enables RLS policies to filter data
}
```

**Why**: Need database operations for client management and RLS context

---

### Phase 4: Update ApiServer

**File**: `include/ApiServer_stateless.hpp`

**Changes**:
```cpp
class ApiServer {
private:
    std::shared_ptr<SupabaseClient> db_;
    std::shared_ptr<AuthManager> auth_;  // NEW: Add auth manager
    // REMOVE: std::string default_client_id_;

    // NEW: Authentication helper (handles auth + error responses)
    bool authenticate_request(const httplib::Request& req,
                             httplib::Response& res,
                             std::string& client_id_out);

public:
    // CHANGE: Remove client_id parameter
    ApiServer(int port,
              std::shared_ptr<SupabaseClient> db,
              std::shared_ptr<AuthManager> auth);
};

// NEW: Authentication macro for cleaner endpoint code
#define REQUIRE_AUTH() \
    std::string client_id; \
    if (!authenticate_request(req, res, client_id)) return;
```

**File**: `src/ApiServer_stateless.cpp`

**New Authentication Helper**:
```cpp
bool ApiServer::authenticate_request(const httplib::Request& req,
                                     httplib::Response& res,
                                     std::string& client_id_out) {
    // 1. Get Authorization header
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) {
        res.status = 401;
        res.set_content(R"({"error": {"code": "MISSING_AUTH", "message": "Authorization header required"}})",
                       "application/json");
        return false;
    }

    // 2. Extract API key ("Bearer sk-xxx")
    auto api_key_opt = AuthManager::extract_api_key(auth_header);
    if (!api_key_opt) {
        res.status = 401;
        res.set_content(R"({"error": {"code": "INVALID_AUTH_FORMAT", "message": "Authorization format must be: Bearer <api-key>"}})",
                       "application/json");
        return false;
    }

    // 3. Authenticate and get client_id
    auto client_id_opt = auth_->authenticate(api_key_opt.value());
    if (!client_id_opt) {
        res.status = 401;
        res.set_content(R"({"error": {"code": "INVALID_API_KEY", "message": "Invalid or expired API key"}})",
                       "application/json");
        return false;
    }

    // 4. Set RLS context for this request
    client_id_out = client_id_opt.value();
    db_->set_client_context(client_id_out);

    return true;
}
```

**Update Every Endpoint**:

Before:
```cpp
server_.Post("/api/sessions/:id/chat", [this](const Request& req, Response& res) {
    auto session_opt = db_->getSession(session_id, default_client_id_);
    // ... rest of handler
});
```

After:
```cpp
server_.Post("/api/sessions/:id/chat", [this](const Request& req, Response& res) {
    REQUIRE_AUTH();  // One line! Handles auth + sets client_id variable

    // client_id is now available, RLS context already set
    auto session_opt = db_->getSession(session_id, client_id);
    // ... rest of handler
});
```

**Why Macro?**
- Eliminates 3 lines of boilerplate at all 26 endpoints
- Impossible to forget the early return
- All auth logic centralized in `authenticate_request()`
- Change auth behavior once, applies everywhere

**Endpoints to Update** (26 locations):
- ✅ POST `/api/sessions` - Create session
- ✅ GET `/api/sessions` - List sessions
- ✅ GET `/api/sessions/:id` - Get session
- ✅ DELETE `/api/sessions/:id` - Delete session
- ✅ POST `/api/sessions/:id/chat` - Send message
- ✅ GET `/api/sessions/:id/history` - Get history
- ✅ DELETE `/api/sessions/:id/history` - Clear history
- ✅ GET `/api/sessions/:id/tools` - Get tools
- ✅ POST `/api/sessions/:id/tools` - Register tools
- ✅ DELETE `/api/sessions/:id/tools/:name` - Unregister tool
- ✅ POST `/api/sessions/:id/tools/agent` - Register agent tool

**New Endpoints**:
```cpp
// POST /api/clients - Create new client (public signup - no auth required!)
server_.Post("/api/clients", [this](const Request& req, Response& res) {
    // NO REQUIRE_AUTH() - this is the signup endpoint!
    json body = json::parse(req.body);
    string display_name = body.value("display_name", "");

    auto result = auth_->create_client(display_name);
    if (!result) {
        res.status = 500;
        res.set_content(R"({"error": "Failed to create client"})", "application/json");
        return;
    }

    auto [client_id, api_key] = result.value();
    json response = {
        {"client_id", client_id},
        {"api_key", api_key},  // ONLY time we return the plain API key!
        {"message", "Save this API key - it won't be shown again"}
    };
    res.set_content(response.dump(), "application/json");
});

// GET /api/clients/me - Get current client info (requires auth)
server_.Get("/api/clients/me", [this](const Request& req, Response& res) {
    REQUIRE_AUTH();  // Now we need auth

    // Return info about the authenticated client
    json response = {
        {"client_id", client_id},
        // Could load more info from database if needed
    };
    res.set_content(response.dump(), "application/json");
});
```

---

### Phase 5: Update Main Entry Point

**File**: `src/api_main_stateless.cpp`

Before:
```cpp
string client_id = "00000000-0000-0000-0000-000000000000";
ApiServer server(port, db, client_id);
```

After:
```cpp
auto auth = std::make_shared<AuthManager>(db);
ApiServer server(port, db, auth);
```

**Remove**:
- ✅ `LOOM_CLIENT_ID` environment variable reading
- ✅ Default client_id logic

---

### Phase 6: Update CMakeLists.txt

**File**: `CMakeLists.txt`

**Add**:
```cmake
# OpenSSL for API key hashing
find_package(OpenSSL REQUIRED)

target_link_libraries(loom_api_stateless
    PRIVATE
    # ... existing libraries ...
    OpenSSL::SSL
    OpenSSL::Crypto
)
```

**Why**: Need OpenSSL for SHA-256 hashing of API keys

---

### Phase 7: Testing Strategy

**Step 1**: Apply migration
```bash
# In Supabase SQL Editor
-- Run migrations/004_enable_multi_tenancy.sql
```

**Step 2**: Create test client
```bash
# Create first client via SQL (bootstrap)
curl -X POST http://localhost:8080/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Test Client"}'

# Response: {"client_id": "uuid", "api_key": "sk-xxxxx"}
```

**Step 3**: Test authentication
```bash
# Try without auth (should fail)
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}'
# Expected: 401 Unauthorized

# Try with auth (should succeed)
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-xxxxx" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}'
# Expected: 200 OK with session_id
```

**Step 4**: Test tenant isolation
```bash
# Create two clients
CLIENT_A_KEY="sk-aaaaaa"
CLIENT_B_KEY="sk-bbbbbb"

# Client A creates session
SESSION_A=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Authorization: Bearer $CLIENT_A_KEY" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt"}' | jq -r '.session_id')

# Client B tries to access Client A's session (should fail)
curl -X GET http://localhost:8080/api/sessions/$SESSION_A \
  -H "Authorization: Bearer $CLIENT_B_KEY"
# Expected: 404 Not Found (RLS filters it out)

# Client A can access it (should succeed)
curl -X GET http://localhost:8080/api/sessions/$SESSION_A \
  -H "Authorization: Bearer $CLIENT_A_KEY"
# Expected: 200 OK with session data
```

---

## Security Considerations

### 1. API Key Format
- Prefix: `sk-` (secret key)
- Length: 32+ random characters
- Alphabet: `a-zA-Z0-9`
- Example: `sk-a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6`

### 2. Storage
- ✅ **NEVER** store plain API keys
- ✅ **ALWAYS** store SHA-256 hash
- ✅ Use `api_key_hash` column in database

### 3. Transmission
- ✅ HTTPS required in production
- ✅ API key in `Authorization: Bearer` header
- ❌ Never in URL query parameters

### 4. Row-Level Security
- ✅ RLS policies enforce isolation at database level
- ✅ Application bugs can't leak data between clients
- ✅ `app.current_client_id` session variable set per request

---

## Migration Path

### Development/Testing
```bash
# 1. Apply migration
# Run 004_enable_multi_tenancy.sql in Supabase

# 2. Update code (all phases above)

# 3. Rebuild
cd build && cmake .. && make

# 4. Create bootstrap client manually in Supabase
INSERT INTO clients (api_key_hash, display_name)
VALUES (
  -- Hash of 'sk-test-key-123'
  'a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3',
  'Bootstrap Client'
);

# 5. Test with known API key
export TEST_API_KEY="sk-test-key-123"
curl -H "Authorization: Bearer $TEST_API_KEY" ...
```

### Production
```bash
# 1. Apply migration to production database

# 2. Deploy new code

# 3. Provide client creation endpoint or admin panel

# 4. Clients get API keys when they sign up

# 5. All requests require Authorization header
```

---

## API Changes for Users

### Before (Single Tenant)
```bash
# No auth needed
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt"}'
```

### After (Multi-Tenant)
```bash
# Auth required
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer sk-your-api-key" \
  -d '{"engine": "chatgpt"}'
```

**Breaking Change**: All existing API calls must add `Authorization` header

---

## Environment Variables

### Remove
- ❌ `LOOM_CLIENT_ID` (no longer needed)

### Keep
- ✅ `SUPABASE_URL`
- ✅ `SUPABASE_KEY`
- ✅ `LOOM_API_PORT`
- ✅ `OPENAI_API_KEY`
- ✅ `BRAVE_SEARCH_API_KEY`
- ✅ `GITHUB_TOKEN`

---

## Files to Create/Modify

### New Files (2)
- ✅ `migrations/004_enable_multi_tenancy.sql`
- ⏳ `include/AuthManager.hpp`
- ⏳ `src/AuthManager.cpp`

### Modified Files (6)
- ⏳ `include/ApiServer_stateless.hpp` - Add auth, remove default_client_id
- ⏳ `src/ApiServer_stateless.cpp` - Add auth to all endpoints
- ⏳ `include/SupabaseClient.hpp` - Add client methods
- ⏳ `src/SupabaseClient.cpp` - Implement client methods
- ⏳ `src/api_main_stateless.cpp` - Remove client_id parameter
- ⏳ `CMakeLists.txt` - Add OpenSSL dependency
- ⏳ `API_USAGE.md` - Update with auth examples

---

## Success Criteria

- ✅ Multiple clients can use the API simultaneously
- ✅ Each client has unique API key
- ✅ Client A cannot access Client B's sessions
- ✅ RLS enforces isolation at database level
- ✅ Unauthorized requests return 401
- ✅ API keys are hashed, never stored plain
- ✅ All existing functionality works with auth

---

## Rollback Plan

If issues arise:
1. Keep migration 002 schema (simplified client_id as TEXT)
2. Add authentication layer without RLS
3. Validate client_id in application code
4. Enable RLS later when ready

---

## Timeline Estimate

- Phase 1 (Migration): ✅ Complete
- Phase 2 (AuthManager): ~2 hours
- Phase 3 (SupabaseClient): ~1 hour
- Phase 4 (ApiServer): ~3 hours
- Phase 5 (Main): ~30 min
- Phase 6 (CMake): ~30 min
- Phase 7 (Testing): ~2 hours

**Total**: ~9 hours of implementation + testing
