#ifndef AGENT_SESSION_MANAGER_HPP
#define AGENT_SESSION_MANAGER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include "Agent.hpp"

struct AgentSession {
    std::string id;
    std::shared_ptr<Agent> agent;
    std::string engine_type;  // "chatgpt" or "ollama"
    std::string model;
    std::chrono::time_point<std::chrono::system_clock> created_at;
};

class AgentSessionManager {
private:
    std::map<std::string, AgentSession> sessions_;

    std::string generate_session_id();

public:
    AgentSessionManager() = default;

    // Create a new agent session
    // Returns the session ID
    std::string create_session(const std::string& engine_type,
                               const std::string& model,
                               bool verbose = false);

    // Get a session by ID
    // Returns nullptr if not found
    AgentSession* get_session(const std::string& id);

    // Destroy a session
    // Returns true if session was found and destroyed
    bool destroy_session(const std::string& id);

    // List all sessions
    std::vector<AgentSession> list_sessions() const;

    // Get number of active sessions
    size_t size() const;
};

#endif // AGENT_SESSION_MANAGER_HPP
