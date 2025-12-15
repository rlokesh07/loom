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
