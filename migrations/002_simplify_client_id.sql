-- Migration to simplify client_id for easier testing
-- This changes client_id from UUID foreign key to TEXT

-- Drop RLS policies first
DROP POLICY IF EXISTS sessions_isolation ON sessions;

-- Disable RLS
ALTER TABLE sessions DISABLE ROW LEVEL SECURITY;
ALTER TABLE child_sessions DISABLE ROW LEVEL SECURITY;

-- Drop foreign key constraint
ALTER TABLE sessions DROP CONSTRAINT IF EXISTS sessions_client_id_fkey;

-- Change client_id to TEXT
ALTER TABLE sessions ALTER COLUMN client_id TYPE TEXT USING client_id::TEXT;

-- Recreate index
DROP INDEX IF EXISTS idx_sessions_client;
CREATE INDEX idx_sessions_client ON sessions(client_id);

-- Note: This simplifies the schema for development/testing
-- For production, you may want to keep the UUID foreign key and RLS
