-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- ============================================
-- CLIENTS TABLE (Multi-tenancy)
-- ============================================
CREATE TABLE clients (
    client_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    api_key_hash TEXT NOT NULL UNIQUE,
    display_name TEXT,

    -- Encrypted credentials (JSON object)
    credentials_encrypted JSONB DEFAULT '{}',

    -- Quotas
    max_sessions INTEGER DEFAULT 100,
    max_requests_per_hour INTEGER DEFAULT 1000,

    -- Timestamps
    created_at TIMESTAMPTZ DEFAULT NOW(),
    last_active_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes
CREATE INDEX idx_clients_api_key ON clients(api_key_hash);
CREATE INDEX idx_clients_last_active ON clients(last_active_at);

-- ============================================
-- SESSIONS TABLE (Configuration templates)
-- ============================================
CREATE TABLE sessions (
    session_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    client_id UUID NOT NULL REFERENCES clients(client_id) ON DELETE CASCADE,

    -- Session configuration
    engine_type TEXT NOT NULL CHECK (engine_type IN ('chatgpt', 'ollama')),
    model TEXT NOT NULL,
    system_prompt TEXT DEFAULT '',

    -- Conversation history stored as JSONB array
    -- Each element: {"role": "user|assistant|system|tool", "content": "...", "tool_calls": [...]}
    conversation_history JSONB DEFAULT '[]'::jsonb,

    -- Tool configuration (array of tool names)
    tool_names TEXT[] DEFAULT ARRAY[]::TEXT[],

    -- Timestamps
    created_at TIMESTAMPTZ DEFAULT NOW(),
    last_accessed_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes
CREATE INDEX idx_sessions_client ON sessions(client_id);
CREATE INDEX idx_sessions_last_accessed ON sessions(last_accessed_at);
CREATE INDEX idx_sessions_client_accessed ON sessions(client_id, last_accessed_at);

-- GIN index for JSONB queries (if you need to search within conversation)
CREATE INDEX idx_sessions_conversation ON sessions USING gin(conversation_history);

-- ============================================
-- CHILD_SESSIONS TABLE (Agent-as-tool)
-- ============================================
CREATE TABLE child_sessions (
    parent_session_id UUID NOT NULL REFERENCES sessions(session_id) ON DELETE CASCADE,
    tool_name TEXT NOT NULL,
    child_session_id UUID NOT NULL REFERENCES sessions(session_id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ DEFAULT NOW(),

    PRIMARY KEY (parent_session_id, tool_name)
);

CREATE INDEX idx_child_sessions_parent ON child_sessions(parent_session_id);

-- ============================================
-- REQUEST_LOG TABLE (Optional - for debugging/billing)
-- ============================================
CREATE TABLE request_log (
    request_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    session_id UUID REFERENCES sessions(session_id) ON DELETE SET NULL,
    client_id UUID REFERENCES clients(client_id) ON DELETE SET NULL,

    -- Request details
    endpoint TEXT NOT NULL,
    request_params JSONB,

    -- Response details
    response_status INTEGER,
    response_data JSONB,
    tool_calls JSONB,

    -- Metrics
    duration_ms INTEGER,
    tokens_used INTEGER,
    success BOOLEAN,
    error_message TEXT,

    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Indexes for analytics
CREATE INDEX idx_request_log_session ON request_log(session_id, created_at DESC);
CREATE INDEX idx_request_log_client ON request_log(client_id, created_at DESC);
CREATE INDEX idx_request_log_created ON request_log(created_at DESC);

-- ============================================
-- FUNCTIONS & TRIGGERS
-- ============================================

-- Function to update last_accessed_at
CREATE OR REPLACE FUNCTION update_last_accessed()
RETURNS TRIGGER AS $$
BEGIN
    UPDATE sessions
    SET last_accessed_at = NOW()
    WHERE session_id = NEW.session_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger on conversation updates
CREATE TRIGGER trigger_update_session_access
    AFTER UPDATE OF conversation_history ON sessions
    FOR EACH ROW
    EXECUTE FUNCTION update_last_accessed();

-- Function to clean up old sessions (can run periodically)
CREATE OR REPLACE FUNCTION cleanup_old_sessions(days_old INTEGER DEFAULT 30)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM sessions
    WHERE last_accessed_at < NOW() - (days_old || ' days')::INTERVAL;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- ============================================
-- ROW LEVEL SECURITY (Multi-tenancy isolation)
-- ============================================

-- Enable RLS
ALTER TABLE sessions ENABLE ROW LEVEL SECURITY;
ALTER TABLE child_sessions ENABLE ROW LEVEL SECURITY;

-- Policy: Clients can only see their own sessions
CREATE POLICY sessions_isolation ON sessions
    FOR ALL
    USING (client_id = current_setting('app.current_client_id', true)::uuid);

-- Note: We'll set app.current_client_id in our C++ code after authentication
