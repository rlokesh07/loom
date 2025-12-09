-- Migration to enable full multi-tenancy
-- This reverses the simplifications from migration 002

-- Step 1: Disable RLS (must do this before dropping policies)
ALTER TABLE clients DISABLE ROW LEVEL SECURITY;
ALTER TABLE sessions DISABLE ROW LEVEL SECURITY;
ALTER TABLE child_sessions DISABLE ROW LEVEL SECURITY;

-- Step 2: Drop any existing RLS policies
DROP POLICY IF EXISTS clients_insert_policy ON clients;
DROP POLICY IF EXISTS clients_select_policy ON clients;
DROP POLICY IF EXISTS sessions_isolation ON sessions;
DROP POLICY IF EXISTS child_sessions_isolation ON child_sessions;

-- Step 3: Drop existing foreign key constraint if it exists
ALTER TABLE sessions DROP CONSTRAINT IF EXISTS sessions_client_id_fkey;

-- Step 4: Change client_id back to UUID type
ALTER TABLE sessions ALTER COLUMN client_id TYPE UUID USING client_id::UUID;

-- Step 5: Re-add foreign key constraint
ALTER TABLE sessions
ADD CONSTRAINT sessions_client_id_fkey
FOREIGN KEY (client_id) REFERENCES clients(client_id) ON DELETE CASCADE;

-- Step 6: Enable Row Level Security
ALTER TABLE clients ENABLE ROW LEVEL SECURITY;
ALTER TABLE sessions ENABLE ROW LEVEL SECURITY;
ALTER TABLE child_sessions ENABLE ROW LEVEL SECURITY;

-- Step 7: Create RLS policies for clients
-- Allow anyone to insert (for signup endpoint)
CREATE POLICY clients_insert_policy ON clients
    FOR INSERT
    WITH CHECK (true);

-- Allow clients to select only their own record
CREATE POLICY clients_select_policy ON clients
    FOR SELECT
    USING (client_id = current_setting('app.current_client_id', true)::uuid);

-- Step 8: Create RLS policies for sessions
-- This policy uses a session variable that will be set by the application
CREATE POLICY sessions_isolation ON sessions
    FOR ALL
    USING (client_id = current_setting('app.current_client_id', true)::uuid);

-- Step 9: Create RLS policies for child_sessions
CREATE POLICY child_sessions_isolation ON child_sessions
    FOR ALL
    USING (
        parent_session_id IN (
            SELECT session_id FROM sessions
            WHERE client_id = current_setting('app.current_client_id', true)::uuid
        )
    );

-- Step 10: Add index for API key lookups
CREATE INDEX IF NOT EXISTS idx_clients_api_key_hash ON clients(api_key_hash);

-- Step 11: Update engine_type constraint to include gemini
ALTER TABLE sessions DROP CONSTRAINT IF EXISTS sessions_engine_type_check;
ALTER TABLE sessions ADD CONSTRAINT sessions_engine_type_check
    CHECK (engine_type IN ('chatgpt', 'ollama', 'gemini'));
