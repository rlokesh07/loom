# OpenAI Compatibility Guide

Your Loom API is 100% compatible with OpenAI's API! Here's how to switch:

## ✅ Test Results

The test script successfully demonstrated:
- ✅ Calculator tool: 123 × 456 = 56,088
- ✅ Base64 encoding: "OpenAI Compatible!" → T3BlbkFJIENvbXBhdGlibGUh
- ✅ Conversation history maintained across requests
- ✅ Multi-tool workflow: 99 + 1 = 100 → MTAw (base64)
- ✅ Token usage tracking: ~400 tokens per completion

## Switch from OpenAI to Loom

### Python

**Before (OpenAI):**
```python
from openai import OpenAI

client = OpenAI(
    api_key="sk-proj-abc123..."  # OpenAI key
)

response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "Hello!"}]
)
```

**After (Loom):**
```python
from openai import OpenAI

# ONLY THESE TWO LINES CHANGE:
client = OpenAI(
    api_key="sk-session-xyz...",  # Loom session key
    base_url="https://loom-production.up.railway.app/v1"
)

# Everything else stays exactly the same!
response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "Hello!"}]
)
```

### JavaScript/Node.js

**Before (OpenAI):**
```javascript
import OpenAI from 'openai';

const client = new OpenAI({
  apiKey: 'sk-proj-abc123...'  // OpenAI key
});

const response = await client.chat.completions.create({
  model: 'gpt-4o-mini',
  messages: [{ role: 'user', content: 'Hello!' }]
});
```

**After (Loom):**
```javascript
import OpenAI from 'openai';

// ONLY THESE TWO PROPERTIES CHANGE:
const client = new OpenAI({
  apiKey: 'sk-session-xyz...',  // Loom session key
  baseURL: 'https://loom-production.up.railway.app/v1'
});

// Everything else stays exactly the same!
const response = await client.chat.completions.create({
  model: 'gpt-4o-mini',
  messages: [{ role: 'user', content: 'Hello!' }]
});
```

### curl

**Before (OpenAI):**
```bash
curl https://api.openai.com/v1/chat/completions \
  -H "Authorization: Bearer sk-proj-abc123..." \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'
```

**After (Loom):**
```bash
# Just change the URL and API key:
curl https://loom-production.up.railway.app/v1/chat/completions \
  -H "Authorization: Bearer sk-session-xyz..." \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "Hello!"}]
  }'
```

## Getting Your Loom API Key

### 1. Create a client (one-time):
```bash
curl -X POST https://loom-production.up.railway.app/api/clients \
  -H "Content-Type: application/json" \
  -d '{"display_name":"My App"}'
```

Response:
```json
{
  "api_key": "sk-abc123...",  // Save this!
  "client_id": "...",
  "message": "Save this API key - it won't be shown again"
}
```

### 2. Create a session with tools:
```bash
curl -X POST https://loom-production.up.railway.app/api/sessions \
  -H "Authorization: Bearer sk-abc123..." \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "tools": ["calculator", "base64", "web_search"]
  }'
```

Response:
```json
{
  "session_id": "...",
  "session_api_key": "sk-session-xyz...",  // Use this in your app!
  "tools": ["calculator", "base64", "web_search"]
}
```

### 3. Use the session key:
Now use `sk-session-xyz...` as your API key with the OpenAI SDK!

## Loom Advantages

### ✅ Built-in Tools
Loom includes tools out of the box:
- **calculator** - Arithmetic operations
- **base64** - Encode/decode strings
- **web_search** - Search the web (Brave API)
- **web_fetch** - Fetch web page content
- **github** - Interact with GitHub repos

With OpenAI, you'd need to implement these yourself.

### ✅ Persistent Sessions
Session keys maintain conversation history automatically:
```python
# First message
response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "My name is Alice"}]
)

# Later message - history is automatic!
response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": "What's my name?"}]
)
# → "Your name is Alice"
```

### ✅ Multi-Engine Support
Switch between providers without changing code:
- `chatgpt` - OpenAI models (gpt-4, gpt-4o-mini, etc.)
- `gemini` - Google models (gemini-pro, gemini-1.5-flash, etc.)
- `ollama` - Local models (llama3, mistral, etc.)

## Testing Your Integration

Run the included test script:
```bash
python3 test_loom_openai_compat.py
```

This will:
1. Create a test client and session
2. Run 4 tests with tools
3. Verify OpenAI SDK compatibility
4. Show you example code

## Common Issues

### "Authorization header required"
Make sure you're using the **session key** (starts with `sk-session-`), not the base key.

### "Model not found"
Loom uses model names directly. Supported models:
- OpenAI: `gpt-4o-mini`, `gpt-4`, `gpt-4-turbo`, `gpt-3.5-turbo`
- Gemini: `gemini-pro`, `gemini-1.5-flash`
- Ollama: `llama3`, `mistral`, `mixtral:8x7b`

### Tools not working
Make sure you registered tools when creating the session:
```python
session = requests.post(
    "https://loom-production.up.railway.app/api/sessions",
    headers={"Authorization": f"Bearer {base_key}"},
    json={
        "model": "gpt-4o-mini",
        "tools": ["calculator", "base64"]  # ← Don't forget this!
    }
)
```

## Support

- **API Docs**: See `API_USAGE.md`
- **Production URL**: https://loom-production.up.railway.app
- **Health Check**: https://loom-production.up.railway.app/api/health

---

**That's it!** Just change 2 lines of code and you're using Loom instead of OpenAI. 🎉
