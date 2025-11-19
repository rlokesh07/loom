# Loom API Usage Guide

## Quick Start

```bash
# Build and run the server
cd build
cmake --build . --target loom_api
./loom_api

# Server runs on http://localhost:8080
```

## Endpoints

### Health Check

```bash
curl http://localhost:8080/api/health
```
```json
{"status": "ok"}
```

---

### Sessions

#### Create Session

```bash
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "chatgpt",
    "model": "gpt-4o-mini",
    "verbose": false
  }'
```
```json
{
  "session_id": "sess-abc123",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z"
}
```

Engine options: `chatgpt`, `ollama`

#### List Sessions

```bash
curl http://localhost:8080/api/sessions
```
```json
{
  "sessions": [...],
  "count": 1
}
```

#### Get Session

```bash
curl http://localhost:8080/api/sessions/sess-abc123
```
```json
{
  "session_id": "sess-abc123",
  "engine": "chatgpt",
  "model": "gpt-4o-mini",
  "created_at": "2025-01-15T10:30:00Z",
  "history_size": 2
}
```

#### Delete Session

```bash
curl -X DELETE http://localhost:8080/api/sessions/sess-abc123
```
```json
{"success": true, "message": "Session deleted"}
```

---

### Chat

#### Send Message

```bash
curl -X POST http://localhost:8080/api/sessions/sess-abc123/chat \
  -H "Content-Type: application/json" \
  -d '{
    "message": "Hello, how are you?",
    "system_prompt": "You are a helpful assistant.",
    "temperature": 0.7,
    "max_tokens": 500
  }'
```
```json
{
  "success": true,
  "message": "I'm doing well, thank you for asking!"
}
```

Only `message` is required. Other fields are optional.

#### Get History

```bash
curl http://localhost:8080/api/sessions/sess-abc123/history
```
```json
{
  "session_id": "sess-abc123",
  "message_count": 3,
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello"},
    {"role": "assistant", "content": "Hi there!"}
  ]
}
```

#### Clear History

```bash
curl -X DELETE http://localhost:8080/api/sessions/sess-abc123/history
```
```json
{"success": true, "message": "History cleared"}
```

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `LOOM_API_PORT` | Server port (default: 8080) |
| `OPENAI_API_KEY` | Required for ChatGPT engine |
| `BRAVE_SEARCH_API_KEY` | Required for WebSearchTool |
| `GITHUB_TOKEN` | Required for GitHubTool |

---

## Example: Complete Chat Session

```bash
# 1. Create session
SESSION=$(curl -s -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}' | jq -r '.session_id')

echo "Session: $SESSION"

# 2. Send message
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "What is 2+2?"}'

# 3. Continue conversation
curl -X POST http://localhost:8080/api/sessions/$SESSION/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "And what is that multiplied by 3?"}'

# 4. Get full history
curl http://localhost:8080/api/sessions/$SESSION/history

# 5. Cleanup
curl -X DELETE http://localhost:8080/api/sessions/$SESSION
```

---

## Using with Ollama

```bash
# Make sure Ollama is running
ollama serve

# Create Ollama session
curl -X POST http://localhost:8080/api/sessions \
  -H "Content-Type: application/json" \
  -d '{
    "engine": "ollama",
    "model": "llama3.1:8b"
  }'
```
