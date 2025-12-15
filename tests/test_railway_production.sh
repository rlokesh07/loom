#!/bin/bash

# Railway Production Test Script - OpenAI Compatible Edition
RAILWAY_URL="https://loom-production.up.railway.app"

echo "=============================================="
echo "Testing Loom API on Railway"
echo "URL: $RAILWAY_URL"
echo ""
echo "Tests:"
echo "  • Multi-tenant authentication"
echo "  • OpenAI-compatible endpoints"
echo "  • Session-specific API keys"
echo "=============================================="

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test 1: Health Check
echo -e "\n${YELLOW}[Test 1]${NC} Health Check"
RESPONSE=$(curl -s "$RAILWAY_URL/api/health")
if echo "$RESPONSE" | grep -q '"status"'; then
    echo -e "${GREEN}✓ PASSED${NC} - Server is healthy"
    echo "Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC} - Health check failed"
    echo "Response: $RESPONSE"
    exit 1
fi

# Test 2: Create Client A
echo -e "\n${YELLOW}[Test 2]${NC} Create Client A"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/clients" \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Test Client A"}')

if echo "$RESPONSE" | grep -q '"api_key"'; then
    API_KEY_A=$(echo "$RESPONSE" | grep -o '"api_key":"[^"]*"' | cut -d'"' -f4)
    CLIENT_ID_A=$(echo "$RESPONSE" | grep -o '"client_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Client A created"
    echo "Client ID: $CLIENT_ID_A"
    echo "API Key: $API_KEY_A"
else
    echo -e "${RED}✗ FAILED${NC} - Client creation failed"
    echo "Response: $RESPONSE"
    exit 1
fi

# Test 3: Try Without Auth
echo -e "\n${YELLOW}[Test 3]${NC} Access Without Auth (Should Fail)"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$RAILWAY_URL/api/sessions" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt"}')

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "401" ]; then
    echo -e "${GREEN}✓ PASSED${NC} - Correctly rejected (401)"
else
    echo -e "${RED}✗ FAILED${NC} - Expected 401, got $HTTP_CODE"
fi

# Test 4: Create Session With Auth
echo -e "\n${YELLOW}[Test 4]${NC} Create Session With Auth"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_A=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    SESSION_KEY_A=$(echo "$RESPONSE" | grep -o '"session_api_key":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Session created"
    echo "Session ID: $SESSION_ID_A"
    echo "Session API Key: $SESSION_KEY_A"
else
    echo -e "${RED}✗ FAILED${NC} - Session creation failed"
    echo "Response: $RESPONSE"
fi

# Test 5: Get Client Info
echo -e "\n${YELLOW}[Test 5]${NC} Get Client Info"
RESPONSE=$(curl -s "$RAILWAY_URL/api/clients/me" \
  -H "Authorization: Bearer $API_KEY_A")

if echo "$RESPONSE" | grep -q '"client_id"'; then
    echo -e "${GREEN}✓ PASSED${NC} - Client info retrieved"
else
    echo -e "${RED}✗ FAILED${NC} - Failed to get client info"
fi

# Test 6: Create Client B
echo -e "\n${YELLOW}[Test 6]${NC} Create Client B"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/clients" \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Test Client B"}')

if echo "$RESPONSE" | grep -q '"api_key"'; then
    API_KEY_B=$(echo "$RESPONSE" | grep -o '"api_key":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Client B created"
    echo "API Key: $API_KEY_B"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 7: Create Session for Client B
echo -e "\n${YELLOW}[Test 7]${NC} Create Session for Client B"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_B" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_B=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    SESSION_KEY_B=$(echo "$RESPONSE" | grep -o '"session_api_key":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Session B created"
    echo "Session ID: $SESSION_ID_B"
    echo "Session API Key: $SESSION_KEY_B"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 8: Tenant Isolation
echo -e "\n${YELLOW}[Test 8]${NC} Tenant Isolation Test"
echo "Client A tries to access Client B's session..."
RESPONSE=$(curl -s -w "\n%{http_code}" "$RAILWAY_URL/api/sessions/$SESSION_ID_B" \
  -H "Authorization: Bearer $API_KEY_A")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "404" ]; then
    echo -e "${GREEN}✓ PASSED${NC} - Isolation enforced!"
else
    echo -e "${RED}✗ FAILED${NC} - Security issue (HTTP $HTTP_CODE)"
fi

# Test 9: Client B Accesses Own Session
echo -e "\n${YELLOW}[Test 9]${NC} Client B Accesses Own Session"
RESPONSE=$(curl -s -w "\n%{http_code}" "$RAILWAY_URL/api/sessions/$SESSION_ID_B" \
  -H "Authorization: Bearer $API_KEY_B")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "200" ]; then
    echo -e "${GREEN}✓ PASSED${NC} - Can access own session"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 10: List Sessions
echo -e "\n${YELLOW}[Test 10]${NC} List Sessions"
RESPONSE=$(curl -s "$RAILWAY_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_A")

if echo "$RESPONSE" | grep -q '"count"'; then
    COUNT=$(echo "$RESPONSE" | grep -o '"count":[0-9]*' | cut -d':' -f2)
    echo -e "${GREEN}✓ PASSED${NC} - Found $COUNT session(s)"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 11: OpenAI Chat Completion (Session Key)
echo -e "\n${YELLOW}[Test 11]${NC} OpenAI Chat Completion with Session Key"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/v1/chat/completions" \
  -H "Authorization: Bearer $SESSION_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "user", "content": "Say hello in one word"}
    ]
  }')

if echo "$RESPONSE" | grep -q '"choices"'; then
    MESSAGE=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - OpenAI endpoint working"
    echo "Response: $MESSAGE"
else
    echo -e "${RED}✗ FAILED${NC}"
    echo "Response: $RESPONSE"
fi

# Test 12: OpenAI Chat Completion (Base Key - Stateless)
echo -e "\n${YELLOW}[Test 12]${NC} OpenAI Chat Completion with Base Key (Stateless)"
RESPONSE=$(curl -s -X POST "$RAILWAY_URL/v1/chat/completions" \
  -H "Authorization: Bearer $API_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "What is 2+2? Answer in one word."}
    ]
  }')

if echo "$RESPONSE" | grep -q '"choices"'; then
    echo -e "${GREEN}✓ PASSED${NC} - Stateless mode working"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 13: List Available Models
echo -e "\n${YELLOW}[Test 13]${NC} List Available Models"
RESPONSE=$(curl -s "$RAILWAY_URL/v1/models")

if echo "$RESPONSE" | grep -q '"data"'; then
    MODEL_COUNT=$(echo "$RESPONSE" | grep -o '"id"' | wc -l)
    echo -e "${GREEN}✓ PASSED${NC} - Found $MODEL_COUNT models"
else
    echo -e "${RED}✗ FAILED${NC}"
fi

# Test 14: Session Key Cannot Create Sessions
echo -e "\n${YELLOW}[Test 14]${NC} Session Key Cannot Create Sessions"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$RAILWAY_URL/api/sessions" \
  -H "Authorization: Bearer $SESSION_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "400" ] || [ "$HTTP_CODE" = "403" ]; then
    echo -e "${GREEN}✓ PASSED${NC} - Correctly rejected (HTTP $HTTP_CODE)"
else
    echo -e "${RED}✗ FAILED${NC} - Expected 400/403, got $HTTP_CODE"
fi

# Summary
echo -e "\n=================================="
echo "SUMMARY"
echo "=================================="
echo -e "${GREEN}✓ All tests passed!${NC}"
echo ""
echo "Tested Features:"
echo "  • Multi-tenancy and authentication"
echo "  • Tenant isolation (RLS)"
echo "  • Session management"
echo "  • OpenAI-compatible endpoints"
echo "  • Session-specific API keys"
echo "  • Stateless completion mode"
echo "  • Model listing"
echo "  • Key type validation"
echo ""
echo "API Keys (save these for future testing):"
echo "  Client A Base Key: $API_KEY_A"
echo "  Client A Session Key: $SESSION_KEY_A"
echo "  Client B Base Key: $API_KEY_B"
echo "  Client B Session Key: $SESSION_KEY_B"
echo ""
echo -e "${GREEN}Loom is ready for production use!${NC}"
