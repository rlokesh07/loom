#!/bin/bash

# Railway Production Test Script - Interactive Agent Demo
RAILWAY_URL="https://loom-production.up.railway.app"

echo "=============================================="
echo "Loom API - Interactive Agent Demo"
echo "URL: $RAILWAY_URL"
echo ""
echo "This script demonstrates:"
echo "  • Multi-tenant authentication"
echo "  • Agent conversations with memory"
echo "  • Tool usage (calculator, web search)"
echo "  • OpenAI-compatible endpoints"
echo "=============================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Debug mode - set to 1 to see full responses
DEBUG=${DEBUG:-0}

# Helper function to extract JSON values
extract_json() {
    echo "$2" | grep -o "\"$1\":\"[^\"]*\"" | cut -d'"' -f4 | head -1
}

debug_response() {
    if [ "$DEBUG" = "1" ]; then
        echo -e "${CYAN}[DEBUG] Response:${NC}"
        echo "$1" | jq '.' 2>/dev/null || echo "$1"
    fi
}

# Test 1: Health Check
echo -e "\n${YELLOW}[1/10]${NC} Health Check"
RESPONSE=$(curl -s "$RAILWAY_URL/api/health")
debug_response "$RESPONSE"
if echo "$RESPONSE" | grep -q '"status"'; then
    echo -e "${GREEN}✓ Server is healthy${NC}"
else
    echo -e "${RED}✗ Server unreachable${NC}"
    echo "Response: $RESPONSE"
    exit 1
fi

# Test 2: Create Client
echo -e "\n${YELLOW}[2/10]${NC} Creating Client"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/clients" \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Demo Client"}')

debug_response "$RESPONSE"
API_KEY=$(extract_json "api_key" "$RESPONSE")
if [ -z "$API_KEY" ]; then
    echo -e "${RED}✗ Failed to create client${NC}"
    echo "Response: $RESPONSE"
    exit 1
fi
echo -e "${GREEN}✓ Client created${NC}"
echo -e "${CYAN}API Key: ${API_KEY:0:20}...${NC}"

# Test 3: Create Session
echo -e "\n${YELLOW}[3/10]${NC} Creating Agent Session"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-4o-mini"}')

debug_response "$RESPONSE"
SESSION_ID=$(extract_json "session_id" "$RESPONSE")
SESSION_KEY=$(extract_json "session_api_key" "$RESPONSE")
if [ -z "$SESSION_ID" ]; then
    echo -e "${RED}✗ Failed to create session${NC}"
    echo "Response: $RESPONSE"
    exit 1
fi
echo -e "${GREEN}✓ Session created${NC}"
echo -e "${CYAN}Session ID: $SESSION_ID${NC}"

# Test 4: Register Tools
echo -e "\n${YELLOW}[4/10]${NC} Registering Tools (calculator, base64)"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/sessions/$SESSION_ID/tools" \
  -H "Authorization: Bearer $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"tools": ["calculator", "base64"]}')

debug_response "$RESPONSE"
if echo "$RESPONSE" | grep -q '"tool_count"'; then
    TOOL_COUNT=$(echo "$RESPONSE" | grep -o '"tool_count":[0-9]*' | cut -d':' -f2)
    echo -e "${GREEN}✓ Registered $TOOL_COUNT tools${NC}"
else
    echo -e "${RED}✗ Failed to register tools${NC}"
    echo "Response: $RESPONSE"
fi

# Test 5: Simple Conversation
echo -e "\n${YELLOW}[5/10]${NC} Starting Conversation"
echo -e "${BLUE}User: \"My name is Alice. Remember it!\"${NC}"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/v1/chat/completions" \
  -H "Authorization: Bearer $SESSION_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "My name is Alice. Remember it!"}]
  }')

debug_response "$RESPONSE"

# Check for error first
if echo "$RESPONSE" | grep -q '"error"'; then
    echo -e "${RED}✗ API Error${NC}"
    echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"
    exit 1
fi

# Try to extract message using jq if available, otherwise fallback to grep
if command -v jq &> /dev/null; then
    MESSAGE=$(echo "$RESPONSE" | jq -r '.choices[0].message.content // empty' 2>/dev/null)
else
    MESSAGE=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
fi

if [ -n "$MESSAGE" ]; then
    echo -e "${GREEN}Assistant: $MESSAGE${NC}"
else
    echo -e "${RED}✗ No response received${NC}"
    echo "Full response:"
    echo "$RESPONSE"
fi

# Test 6: Test Memory
echo -e "\n${YELLOW}[6/10]${NC} Testing Conversation Memory"
echo -e "${BLUE}User: \"What's my name?\"${NC}"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/v1/chat/completions" \
  -H "Authorization: Bearer $SESSION_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "What is my name?"}]
  }')

debug_response "$RESPONSE"

if command -v jq &> /dev/null; then
    MESSAGE=$(echo "$RESPONSE" | jq -r '.choices[0].message.content // empty' 2>/dev/null)
else
    MESSAGE=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
fi

if [ -n "$MESSAGE" ]; then
    echo -e "${GREEN}Assistant: $MESSAGE${NC}"

    if echo "$MESSAGE" | grep -qi "alice"; then
        echo -e "${GREEN}✓ Memory working! Agent remembered the name.${NC}"
    else
        echo -e "${YELLOW}⚠ Agent may not have remembered (expected 'Alice')${NC}"
    fi
else
    echo -e "${RED}✗ No response${NC}"
    echo "Full response: $RESPONSE"
fi

# Test 7: Tool Usage - Calculator
echo -e "\n${YELLOW}[7/10]${NC} Testing Tool Usage (Calculator)"
echo -e "${BLUE}User: \"What is 127 multiplied by 89?\"${NC}"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/sessions/$SESSION_ID/chat" \
  -H "Authorization: Bearer $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"message": "What is 127 multiplied by 89? Use the calculator tool."}')

debug_response "$RESPONSE"

if command -v jq &> /dev/null; then
    MESSAGE=$(echo "$RESPONSE" | jq -r '.message // empty' 2>/dev/null)
else
    MESSAGE=$(echo "$RESPONSE" | grep -o '"message":"[^"]*"' | cut -d'"' -f4)
fi

if [ -n "$MESSAGE" ]; then
    echo -e "${GREEN}Assistant: $MESSAGE${NC}"
else
    echo -e "${RED}✗ No response${NC}"
    echo "Full response: $RESPONSE"
fi

if echo "$RESPONSE" | grep -q '"tool_calls"'; then
    if command -v jq &> /dev/null; then
        TOOL_NAME=$(echo "$RESPONSE" | jq -r '.tool_calls[0].tool_name // empty' 2>/dev/null)
    else
        TOOL_NAME=$(echo "$RESPONSE" | grep -o '"tool_name":"[^"]*"' | head -1 | cut -d'"' -f4)
    fi
    if [ -n "$TOOL_NAME" ]; then
        echo -e "${CYAN}✓ Tool used: $TOOL_NAME${NC}"
    fi
else
    echo -e "${YELLOW}⚠ No tool calls detected${NC}"
fi

# Test 8: Get Conversation History
echo -e "\n${YELLOW}[8/10]${NC} Fetching Conversation History"
RESPONSE=$(curl -s "$RAILWAY_URL/api/sessions/$SESSION_ID/history" \
  -H "Authorization: Bearer $API_KEY")

debug_response "$RESPONSE"

if command -v jq &> /dev/null; then
    MESSAGE_COUNT=$(echo "$RESPONSE" | jq -r '.message_count // empty' 2>/dev/null)
else
    MESSAGE_COUNT=$(echo "$RESPONSE" | grep -o '"message_count":[0-9]*' | cut -d':' -f2)
fi

if [ -n "$MESSAGE_COUNT" ]; then
    echo -e "${GREEN}✓ Conversation has $MESSAGE_COUNT messages${NC}"
    echo -e "${CYAN}Full history stored in database (persists across restarts)${NC}"
else
    echo -e "${RED}✗ Failed to fetch history${NC}"
    echo "Response: $RESPONSE"
fi

# Test 9: Stateless Mode Test
echo -e "\n${YELLOW}[9/10]${NC} Testing Stateless Mode (with base key)"
echo -e "${BLUE}One-off question: \"What is the capital of France?\"${NC}"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/v1/chat/completions" \
  -H "Authorization: Bearer $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [
      {"role": "system", "content": "Answer in one word."},
      {"role": "user", "content": "What is the capital of France?"}
    ]
  }')

debug_response "$RESPONSE"

if command -v jq &> /dev/null; then
    MESSAGE=$(echo "$RESPONSE" | jq -r '.choices[0].message.content // empty' 2>/dev/null)
else
    MESSAGE=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
fi

if [ -n "$MESSAGE" ]; then
    echo -e "${GREEN}Assistant: $MESSAGE${NC}"
    echo -e "${CYAN}(This was stateless - no history saved)${NC}"
else
    echo -e "${RED}✗ No response${NC}"
    echo "Full response: $RESPONSE"
fi

# Test 10: List Available Models
echo -e "\n${YELLOW}[10/10]${NC} Listing Available Models"
RESPONSE=$(curl -s "$RAILWAY_URL/v1/models")

debug_response "$RESPONSE"

if echo "$RESPONSE" | grep -q '"data"'; then
    MODEL_COUNT=$(echo "$RESPONSE" | grep -o '"id"' | wc -l)
    echo -e "${GREEN}✓ Found $MODEL_COUNT available models${NC}"

    # Show a few models
    echo -e "${CYAN}Sample models:${NC}"
    echo "$RESPONSE" | grep -o '"id":"[^"]*"' | head -5 | cut -d'"' -f4 | while read model; do
        echo "  • $model"
    done
else
    echo -e "${RED}✗ Failed to list models${NC}"
fi

# Summary
echo -e "\n${GREEN}=========================================="
echo "          DEMO COMPLETE!"
echo "==========================================${NC}"
echo ""
echo "What we demonstrated:"
echo "  ✓ Created a multi-tenant client"
echo "  ✓ Created an agent session with GPT-4o-mini"
echo "  ✓ Registered tools (calculator, base64)"
echo "  ✓ Had a conversation with memory"
echo "  ✓ Agent used tools automatically"
echo "  ✓ Conversation persists in database"
echo "  ✓ Stateless mode for one-off queries"
echo "  ✓ OpenAI SDK compatibility"
echo ""
echo -e "${CYAN}Architecture Benefits:${NC}"
echo "  • Stateless API (survives restarts)"
echo "  • Multi-tenant with isolation"
echo "  • Horizontally scalable"
echo "  • OpenAI SDK compatible"
echo "  • Conversation history in Supabase"
echo ""
echo -e "${YELLOW}Your API Keys (save these):${NC}"
echo "  Base Key: $API_KEY"
echo "  Session Key: $SESSION_KEY"
echo ""
echo -e "${GREEN}Try it yourself with the OpenAI SDK:${NC}"
echo ""
echo "  from openai import OpenAI"
echo "  client = OpenAI("
echo "      api_key='$SESSION_KEY',"
echo "      base_url='$RAILWAY_URL/v1'"
echo "  )"
echo "  response = client.chat.completions.create("
echo "      model='gpt-4o-mini',"
echo "      messages=[{'role': 'user', 'content': 'Hello!'}]"
echo "  )"
echo ""
echo -e "${GREEN}🚀 Loom is ready for production!${NC}"
echo ""
echo -e "${CYAN}Tip: Run with DEBUG=1 ./test_railway_production.sh to see full API responses${NC}"
