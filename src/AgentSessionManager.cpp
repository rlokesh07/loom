#include "../include/AgentSessionManager.hpp"
#include "../include/ChatGPTCompletionEngine.hpp"
#include "../include/OllamaCompletionEngine.hpp"
#include "../include/ToolRegistry.hpp"
#include <random>
#include <sstream>
#include <stdexcept>

std::string AgentSessionManager::generate_session_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::string id;
    do {
        std::stringstream ss;
        ss << "sess-" << std::hex << dis(gen);
        id = ss.str();
    } while (sessions_.find(id) != sessions_.end());

    return id;
}

std::string AgentSessionManager::create_session(const std::string& engine_type,
                                                 const std::string& model,
                                                 bool verbose) {
    std::shared_ptr<Agent> agent;

    if (engine_type == "ollama") {
        auto engine = std::make_shared<OllamaCompletionEngine>();
        agent = std::make_shared<Agent>(engine, model, verbose);
    } else if (engine_type == "chatgpt") {
        auto engine = std::make_shared<ChatGPTCompletionEngine>();
        agent = std::make_shared<Agent>(engine, model, verbose);
    } else {
        throw std::invalid_argument("Invalid engine type: " + engine_type +
                                    ". Must be 'chatgpt' or 'ollama'");
    }

    // Initialize empty tool registry for the agent
    auto registry = std::make_shared<ToolRegistry>();
    agent->set_tool_registry(registry);

    AgentSession session;
    session.id = generate_session_id();
    agent->set_session_id(session.id);
    agent->set_agent_session_manager(this);

    session.agent = agent;
    session.engine_type = engine_type;
    session.model = model;
    session.created_at = std::chrono::system_clock::now();

    sessions_[session.id] = session;

    return session.id;
}

AgentSession* AgentSessionManager::get_session(const std::string& id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return &(it->second);
}

bool AgentSessionManager::destroy_session(const std::string& id) {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return false;
    }

    // Recursively destroy all child sessions first
    for (const auto& [tool_name, child_id] : it->second.child_sessions) {
        destroy_session(child_id);
    }

    // Now destroy this session
    sessions_.erase(it);
    return true;
}

std::vector<AgentSession> AgentSessionManager::list_sessions() const {
    std::vector<AgentSession> result;
    result.reserve(sessions_.size());

    for (const auto& [id, session] : sessions_) {
        result.push_back(session);
    }

    return result;
}

size_t AgentSessionManager::size() const {
    return sessions_.size();
}
