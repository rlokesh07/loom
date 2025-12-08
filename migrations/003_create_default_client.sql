-- Create a default client for testing
-- This assumes you've already run migration 001

-- Disable RLS temporarily for this insert
ALTER TABLE sessions DISABLE ROW LEVEL SECURITY;
ALTER TABLE child_sessions DISABLE ROW LEVEL SECURITY;

-- Create default client with known UUID
INSERT INTO clients (client_id, api_key_hash, display_name)
VALUES (
    '00000000-0000-0000-0000-000000000000'::uuid,
    'default-client-hash',
    'Default Test Client'
)
ON CONFLICT (client_id) DO NOTHING;

-- Note: RLS is disabled for easier testing
-- For production, you would want to re-enable it and set proper policies
