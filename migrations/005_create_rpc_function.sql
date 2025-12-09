-- Create RPC function to set client context for RLS
-- This function sets the session variable that RLS policies use

CREATE OR REPLACE FUNCTION set_client_context(p_client_id uuid)
RETURNS void AS $$
BEGIN
  -- Set the session variable for RLS policies
  -- The 'true' parameter makes it local to the current transaction
  PERFORM set_config('app.current_client_id', p_client_id::text, true);
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Grant execute permission to authenticated users
-- Note: In production, you may want to restrict this further
GRANT EXECUTE ON FUNCTION set_client_context(uuid) TO anon;
GRANT EXECUTE ON FUNCTION set_client_context(uuid) TO authenticated;
