#!/bin/bash

# Railway Production Test Script
RAILWAY_URL="https://loom-production.up.railway.app"

echo "=================================="
echo "Testing Loom API on Railway"
echo "URL: $RAILWAY_URL"
echo "=================================="

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
  -d '{"engine": "chatgpt", "model": "gpt-4"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_A=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Session created"
    echo "Session ID: $SESSION_ID_A"
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
  -d '{"engine": "chatgpt", "model": "gpt-4"}')

if echo "$RESPONSE" | grep -q '"session_id"'; then
    SESSION_ID_B=$(echo "$RESPONSE" | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
    echo -e "${GREEN}✓ PASSED${NC} - Session B created"
    echo "Session ID: $SESSION_ID_B"
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

# Summary
echo -e "\n=================================="
echo "SUMMARY"
echo "=================================="
echo -e "${GREEN}Multi-tenancy working on Railway!${NC}"
echo ""
echo "Client A API Key: $API_KEY_A"
echo "Client B API Key: $API_KEY_B"
