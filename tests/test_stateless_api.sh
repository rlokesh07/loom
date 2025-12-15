#!/bin/bash

# Loom API Stateless Test Script - Comprehensive Suite
API_HOST="localhost"
API_PORT="8080"
API_BASE_URL="http://$API_HOST:$API_PORT"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test counters
PASSED=0
FAILED=0

echo "╔════════════════════════════════════════════════════════╗"
echo "║   Loom API Server - Comprehensive Test Suite          ║"
echo "║                                                        ║"
echo "║   • Multi-Tenancy & Authentication                     ║"
echo "║   • OpenAI-Compatible Endpoints                        ║"
echo "║   • Session-Specific API Keys                          ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Test 0: Health Check
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 0: Health Check                             ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s "$API_BASE_URL/api/health")
if echo "$RESPONSE" | grep -q '"status".*"ok"'; then
    echo -e "${GREEN}✓ Server is healthy${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to connect to API server${NC}"
    ((FAILED++))
    echo "Cannot continue without healthy server. Exiting..."
    exit 1
fi

# Test 1: Create Client A
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 1: Create Client - Test Client A            ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/api/clients" \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Test Client A"}')

if echo "$RESPONSE" | grep -q '"api_key"'; then
    API_KEY_A=$(echo "$RESPONSE" | grep -o '"api_key":"[^"]*"' | cut -d'"' -f4)
    CLIENT_ID_A=$(echo "$RESPONSE" | grep -o '"client_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Client created: $CLIENT_ID_A${NC}"
    echo -e "${GREEN}✓ API Key: $API_KEY_A${NC}"
    echo -e "${GREEN}✓ Save this API key - it won't be shown again!${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Client creation failed${NC}"
    echo "Response: $RESPONSE"
    ((FAILED++))
    echo "❌ Cannot continue without Client A. Exiting..."
    exit 1
fi

# Test 2: Access Without Auth (Should Fail)
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 2: Access Without Auth (Should Fail)        ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$API_BASE_URL/api/sessions" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt"}')

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)
if [ "$HTTP_CODE" = "401" ]; then
    ERROR_CODE=$(echo "$BODY" | grep -o '"error":"[^"]*"' | cut -d'"' -f4)
    MESSAGE=$(echo "$BODY" | grep -o '"message":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Correctly rejected: $ERROR_CODE${NC}"
    echo -e "${GREEN}✓ Message: $MESSAGE${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Expected 401, got $HTTP_CODE${NC}"
    ((FAILED++))
fi

# Test 3: Create Session With Auth - Client A
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 3: Create Session With Auth - Client A      ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_A=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    SESSION_KEY_A=$(echo "$RESPONSE" | grep -o '"session_api_key":"[^"]*"' | cut -d'"' -f4)
    ENGINE=$(echo "$RESPONSE" | grep -o '"engine":"[^"]*"' | cut -d'"' -f4)
    MODEL=$(echo "$RESPONSE" | grep -o '"model":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Session created: $SESSION_ID_A${NC}"
    echo -e "${GREEN}✓ Session API Key: $SESSION_KEY_A${NC}"
    echo -e "${GREEN}✓ Engine: $ENGINE${NC}"
    echo -e "${GREEN}✓ Model: $MODEL${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Session creation failed${NC}"
    echo "Response: $RESPONSE"
    ((FAILED++))
fi

# Test 4: Get Client Info - Client A
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 4: Get Client Info - Client A               ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s "$API_BASE_URL/api/clients/me" \
  -H "Authorization: Bearer $API_KEY_A")

if echo "$RESPONSE" | grep -q '"client_id"'; then
    CLIENT_ID=$(echo "$RESPONSE" | grep -o '"client_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Client ID: $CLIENT_ID${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to get client info${NC}"
    ((FAILED++))
fi

# Test 5: Create Client B
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 5: Create Client - Test Client B            ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/api/clients" \
  -H "Content-Type: application/json" \
  -d '{"display_name": "Test Client B"}')

if echo "$RESPONSE" | grep -q '"api_key"'; then
    API_KEY_B=$(echo "$RESPONSE" | grep -o '"api_key":"[^"]*"' | cut -d'"' -f4)
    CLIENT_ID_B=$(echo "$RESPONSE" | grep -o '"client_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Client created: $CLIENT_ID_B${NC}"
    echo -e "${GREEN}✓ API Key: $API_KEY_B${NC}"
    echo -e "${GREEN}✓ Save this API key - it won't be shown again!${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Client creation failed${NC}"
    ((FAILED++))
fi

# Test 6: Create Session With Auth - Client B
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 6: Create Session With Auth - Client B      ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_B" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_B=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    SESSION_KEY_B=$(echo "$RESPONSE" | grep -o '"session_api_key":"[^"]*"' | cut -d'"' -f4)
    ENGINE=$(echo "$RESPONSE" | grep -o '"engine":"[^"]*"' | cut -d'"' -f4)
    MODEL=$(echo "$RESPONSE" | grep -o '"model":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Session created: $SESSION_ID_B${NC}"
    echo -e "${GREEN}✓ Session API Key: $SESSION_KEY_B${NC}"
    echo -e "${GREEN}✓ Engine: $ENGINE${NC}"
    echo -e "${GREEN}✓ Model: $MODEL${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Session creation failed${NC}"
    ((FAILED++))
fi

# Test 7: Tenant Isolation
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 7: Tenant Isolation                         ║"
echo "╚══════════════════════════════════════════════════╝"
echo "Attempting Client A to access Client B's session..."
RESPONSE=$(curl -s -w "\n%{http_code}" "$API_BASE_URL/api/sessions/$SESSION_ID_B" \
  -H "Authorization: Bearer $API_KEY_A")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "404" ]; then
    echo -e "${GREEN}✓ ✓ Isolation enforced: Client A cannot see Client B's session (404)${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Security issue: Expected 404, got $HTTP_CODE${NC}"
    ((FAILED++))
fi

# Test 8: Client B Accesses Own Session
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 8: Client B Accesses Own Session            ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -w "\n%{http_code}" "$API_BASE_URL/api/sessions/$SESSION_ID_B" \
  -H "Authorization: Bearer $API_KEY_B")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
if [ "$HTTP_CODE" = "200" ]; then
    echo -e "${GREEN}✓ Client B can access own session: $SESSION_ID_B${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to access own session${NC}"
    ((FAILED++))
fi

# Test 9: List Sessions - Client A
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 9: List Sessions - Client A                 ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s "$API_BASE_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_A")

if echo "$RESPONSE" | grep -q '"count"'; then
    COUNT=$(echo "$RESPONSE" | grep -o '"count":[0-9]*' | cut -d':' -f2)
    echo -e "${GREEN}✓ Found $COUNT session(s) for Client A${NC}"
    echo -e "${GREEN}✓ (Should only see own sessions)${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to list sessions${NC}"
    ((FAILED++))
fi

# Test 10: List Sessions - Client B
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 10: List Sessions - Client B                ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s "$API_BASE_URL/api/sessions" \
  -H "Authorization: Bearer $API_KEY_B")

if echo "$RESPONSE" | grep -q '"count"'; then
    COUNT=$(echo "$RESPONSE" | grep -o '"count":[0-9]*' | cut -d':' -f2)
    echo -e "${GREEN}✓ Found $COUNT session(s) for Client B${NC}"
    echo -e "${GREEN}✓ (Should only see own sessions)${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to list sessions${NC}"
    ((FAILED++))
fi

# Test 11: OpenAI Chat Completion (Session Key)
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 11: OpenAI Chat Completion (Session Key)    ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/v1/chat/completions" \
  -H "Authorization: Bearer $SESSION_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role": "user", "content": "Say hello in one word"}
    ]
  }')

if echo "$RESPONSE" | grep -q '"choices"'; then
    CONTENT=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
    COMPLETION_ID=$(echo "$RESPONSE" | grep -o '"id":"[^"]*"' | head -1 | cut -d'"' -f4)
    echo -e "${GREEN}✓ Got response: $CONTENT${NC}"
    echo -e "${GREEN}✓ Chat completion ID: $COMPLETION_ID${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ OpenAI chat completion failed${NC}"
    echo "Response: $RESPONSE"
    ((FAILED++))
fi

# Test 12: OpenAI Chat Completion (Base Key - Stateless)
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 12: OpenAI Chat Completion (Base Key - Stateless) ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -X POST "$API_BASE_URL/v1/chat/completions" \
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
    CONTENT=$(echo "$RESPONSE" | grep -o '"content":"[^"]*"' | head -1 | cut -d'"' -f4)
    echo -e "${GREEN}✓ Got response: $CONTENT${NC}"
    echo -e "${GREEN}✓ Stateless mode working correctly${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Stateless completion failed${NC}"
    ((FAILED++))
fi

# Test 13: List Available Models
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 13: List Available Models                   ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s "$API_BASE_URL/v1/models")

if echo "$RESPONSE" | grep -q '"data"'; then
    MODEL_COUNT=$(echo "$RESPONSE" | grep -o '"id"' | wc -l)
    echo -e "${GREEN}✓ Found $MODEL_COUNT available models${NC}"

    # Show first 5 models
    MODELS=$(echo "$RESPONSE" | grep -o '"id":"[^"]*"' | head -5)
    while IFS= read -r line; do
        MODEL_ID=$(echo "$line" | cut -d'"' -f4)
        ENGINE=$(echo "$RESPONSE" | grep -A2 "\"id\":\"$MODEL_ID\"" | grep -o '"loom_engine":"[^"]*"' | cut -d'"' -f4)
        echo -e "${GREEN}✓   - $MODEL_ID (engine: $ENGINE)${NC}"
    done <<< "$MODELS"
    ((PASSED++))
else
    echo -e "${RED}✗ Failed to list models${NC}"
    ((FAILED++))
fi

# Test 14: Session Key Cannot Create Sessions
echo ""
echo "╔══════════════════════════════════════════════════╗"
echo "║ Test 14: Session Key Cannot Create Sessions      ║"
echo "╚══════════════════════════════════════════════════╝"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$API_BASE_URL/api/sessions" \
  -H "Authorization: Bearer $SESSION_KEY_A" \
  -H "Content-Type: application/json" \
  -d '{"engine": "chatgpt", "model": "gpt-3.5-turbo"}')

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)
if [ "$HTTP_CODE" = "400" ] || [ "$HTTP_CODE" = "403" ]; then
    MESSAGE=$(echo "$BODY" | grep -o '"error":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ Correctly rejected: $MESSAGE${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Expected 400/403, got $HTTP_CODE${NC}"
    ((FAILED++))
fi

# Summary
echo ""
echo "╔════════════════════════════════════════════════════════╗"
echo "║                    TEST SUMMARY                        ║"
echo "╠════════════════════════════════════════════════════════╣"
printf "║  Total:  %-2d tests                                         ║\n" $((PASSED + FAILED))
printf "║  ${GREEN}Passed: %-2d tests ✓${NC}                                     ║\n" $PASSED
printf "║  ${RED}Failed: %-2d tests ✗${NC}                                     ║\n" $FAILED
echo "╚════════════════════════════════════════════════════════╝"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎉 All tests passed! Multi-tenancy is working correctly!"
    exit 0
else
    echo "❌ Some tests failed. Please check the output above."
    exit 1
fi
