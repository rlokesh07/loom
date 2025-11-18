# GitHub Tool Implementation Plan

## Overview
Create a `GitHubTool` that allows AI agents to interact with the GitHub API to perform common operations like listing repositories, reading files, creating issues, and more.

## Architecture

### 1. GitHubTool Class Structure

```cpp
class GitHubTool : public Tool {
private:
    std::string api_token_;
    std::string base_url_ = "https://api.github.com";

    // Helper methods
    std::string make_github_request(const std::string& endpoint,
                                   const std::string& method = "GET",
                                   const json& body = json(nullptr));
    std::string load_token_from_env();

public:
    GitHubTool();
    ToolResult execute(const std::map<std::string, std::any>& params) override;
};
```

### 2. Supported Operations

The tool will support multiple operations via an `operation` parameter:

**Repository Operations:**
- `list_repos` - List repositories for a user/org
- `get_repo` - Get repository information
- `list_branches` - List branches in a repository

**File Operations:**
- `get_file` - Get contents of a file
- `list_files` - List files in a directory
- `search_code` - Search for code in repositories

**Issue Operations:**
- `list_issues` - List issues in a repository
- `get_issue` - Get specific issue details
- `create_issue` - Create a new issue
- `update_issue` - Update an existing issue
- `add_comment` - Add comment to an issue

**Pull Request Operations:**
- `list_prs` - List pull requests
- `get_pr` - Get pull request details
- `create_pr` - Create a pull request

### 3. Parameter Schema

```cpp
// Required parameters
- operation (string): The GitHub operation to perform
- owner (string): Repository owner (user/org name)
- repo (string): Repository name

// Optional parameters (depending on operation)
- path (string): File/directory path
- branch (string): Branch name (default: main)
- issue_number (integer): Issue/PR number
- title (string): Issue/PR title
- body (string): Issue/PR/comment body
- query (string): Search query
- per_page (integer): Results per page (default: 30)
- page (integer): Page number (default: 1)
```

### 4. Authentication

- Use Personal Access Token (PAT) from environment variable `GITHUB_TOKEN`
- Send token in `Authorization: Bearer TOKEN` header
- Support token validation on initialization
- Graceful error handling for missing/invalid tokens

**Authentication Format:**
```
Authorization: Bearer ghp_xxxxxxxxxxxxxxxxxxxx
```

**Rate Limits:**
- 5000 requests per hour with authentication
- 60 requests per hour without authentication

### 5. Implementation Details

**Files to create:**
- `include/GitHubTool.hpp` - Header file
- `src/GitHubTool.cpp` - Implementation
- `examples/github_tool_example.cpp` - Usage example

**Key features:**
- Reuse existing CURL infrastructure from ChatGPT tools
- JSON parsing for GitHub API responses
- Rate limit awareness (5000 req/hour)
- Proper error handling for common scenarios:
  - 404 Not Found
  - 403 Forbidden (rate limit/permissions)
  - 401 Unauthorized (bad token)
  - Network errors

### 6. Example Usage

```cpp
// Create GitHub tool
auto github = std::make_shared<GitHubTool>();

// Register with registry
auto registry = std::make_shared<ToolRegistry>();
registry->register_tool(github);

// Agent can now use GitHub tool
Agent agent(true);
agent.set_tool_registry(registry);

// Example: "List all the issues in the torvalds/linux repository"
auto result = agent.complete(
    "List all open issues in the torvalds/linux repository"
);
```

### 7. Response Format

Each operation returns structured data:

```json
{
  "success": true,
  "operation": "get_file",
  "data": {
    "name": "README.md",
    "path": "README.md",
    "size": 1234,
    "content": "base64_encoded_content",
    "encoding": "base64"
  }
}
```

### 8. Implementation Steps

**Phase 1: Basic Infrastructure**
1. Create header file with class definition
2. Implement constructor with token loading
3. Implement HTTP helper using libcurl
4. Implement ONE operation: `get_file`
5. Test with real GitHub API

**Phase 2: Repository Operations**
6. Add `list_repos` operation
7. Add `get_repo` operation
8. Add `list_branches` operation

**Phase 3: Issue Operations**
9. Add `list_issues` operation
10. Add `get_issue` operation
11. Add `create_issue` operation
12. Add `add_comment` operation

**Phase 4: Advanced Operations**
13. Add `search_code` operation
14. Add `list_prs` operation
15. Add `create_pr` operation

**Phase 5: Testing & Documentation**
16. Create comprehensive example
17. Update CMakeLists.txt
18. Add error handling tests
19. Document all operations

### 9. Testing Strategy

- Test each operation individually
- Test agent autonomously choosing correct operations
- Test error handling (invalid repo, missing permissions)
- Test multi-step workflows (search code → read file → create issue)

### 10. Advantages for AI Agents

With this tool, agents can:
- Research codebases autonomously
- Gather context about repositories
- Create/manage issues based on code analysis
- Search for specific implementations
- Navigate repository structures
- Read documentation and code files

## API Endpoints Reference

### Repository Operations
```
GET  /users/{username}/repos          - List user repositories
GET  /orgs/{org}/repos                - List organization repositories
GET  /repos/{owner}/{repo}            - Get repository information
GET  /repos/{owner}/{repo}/branches   - List branches
```

### File Operations
```
GET  /repos/{owner}/{repo}/contents/{path}  - Get file contents
GET  /search/code?q={query}                 - Search code
```

### Issue Operations
```
GET    /repos/{owner}/{repo}/issues                      - List issues
GET    /repos/{owner}/{repo}/issues/{issue_number}       - Get issue
POST   /repos/{owner}/{repo}/issues                      - Create issue
PATCH  /repos/{owner}/{repo}/issues/{issue_number}       - Update issue
POST   /repos/{owner}/{repo}/issues/{issue_number}/comments - Add comment
```

### Pull Request Operations
```
GET   /repos/{owner}/{repo}/pulls              - List pull requests
GET   /repos/{owner}/{repo}/pulls/{pr_number}  - Get pull request
POST  /repos/{owner}/{repo}/pulls              - Create pull request
```

## Environment Setup

```bash
# Create a Personal Access Token at: https://github.com/settings/tokens
export GITHUB_TOKEN='ghp_your_personal_access_token_here'
```

## Required Scopes for PAT

- `repo` - Full control of private repositories
- `public_repo` - Access public repositories
- `read:org` - Read org and team membership
- `user:email` - Access user email addresses

## Benefits

✅ Enables agents to interact with GitHub programmatically
✅ Supports both read and write operations
✅ Extensible design - easy to add new operations
✅ Follows same pattern as CalculatorTool
✅ Automatic operation selection by LLM
✅ Rich error messages for debugging
✅ Rate limit aware
✅ Secure token-based authentication

## Future Enhancements

- Webhook support
- GitHub Actions integration
- Repository creation/deletion
- Branch protection rules
- Commit operations
- Release management
- Organization management
- Team management
- GraphQL API support
