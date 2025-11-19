#include "ApiServer.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// Helper to format timestamp as ISO 8601
std::string format_timestamp(const std::chrono::time_point<std::chrono::system_clock>& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

ApiServer::ApiServer(int port) : port_(port) {
    configure_cors();
    configure_routes();
}

void ApiServer::configure_cors() {
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // Handle preflight requests
    server_.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });
}

void ApiServer::configure_routes() {
    // Health check
    server_.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({"status": "ok"})", "application/json");
    });

    // POST /api/sessions - Create new session
    server_.Post("/api/sessions", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            std::string engine = body.value("engine", "chatgpt");
            std::string model = body.value("model", "gpt-3.5-turbo");
            bool verbose = body.value("verbose", false);

            std::string session_id = session_manager_.create_session(engine, model, verbose);
            AgentSession* session = session_manager_.get_session(session_id);

            json response = {
                {"session_id", session_id},
                {"engine", session->engine_type},
                {"model", session->model},
                {"created_at", format_timestamp(session->created_at)}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::invalid_argument& e) {
            json error = {
                {"error", {
                    {"code", "INVALID_ENGINE"},
                    {"message", e.what()}
                }}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"error", {
                    {"code", "INTERNAL_ERROR"},
                    {"message", e.what()}
                }}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /api/sessions - List all sessions
    server_.Get("/api/sessions", [this](const httplib::Request& req, httplib::Response& res) {
        auto sessions = session_manager_.list_sessions();

        json sessions_array = json::array();
        for (const auto& session : sessions) {
            sessions_array.push_back({
                {"session_id", session.id},
                {"engine", session.engine_type},
                {"model", session.model},
                {"created_at", format_timestamp(session.created_at)}
            });
        }

        json response = {
            {"sessions", sessions_array},
            {"count", sessions.size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // GET /api/sessions/:id - Get session details
    server_.Get(R"(/api/sessions/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        AgentSession* session = session_manager_.get_session(session_id);

        if (!session) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        json response = {
            {"session_id", session->id},
            {"engine", session->engine_type},
            {"model", session->model},
            {"created_at", format_timestamp(session->created_at)},
            {"history_size", session->agent->get_history().size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // DELETE /api/sessions/:id - Delete session
    server_.Delete(R"(/api/sessions/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        bool destroyed = session_manager_.destroy_session(session_id);

        if (!destroyed) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        json response = {
            {"success", true},
            {"message", "Session deleted"}
        };

        res.set_content(response.dump(), "application/json");
    });

    // POST /api/sessions/:id/chat - Send message
    server_.Post(R"(/api/sessions/([^/]+)/chat)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        AgentSession* session = session_manager_.get_session(session_id);

        if (!session) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);

            std::string message = body.at("message");

            std::map<std::string, std::any> params;
            params["message"] = message;

            if (body.contains("system_prompt")) {
                params["system_prompt"] = body["system_prompt"].get<std::string>();
            }
            if (body.contains("temperature")) {
                params["temperature"] = body["temperature"].get<double>();
            }
            if (body.contains("max_tokens")) {
                params["max_tokens"] = body["max_tokens"].get<int>();
            }

            auto result = session->agent->execute_safe(params);

            json response = {
                {"success", result.is_success()},
                {"message", result.get_message()}
            };

            if (!result.is_success() && result.get_error()) {
                response["error"] = *result.get_error();
            }

            res.set_content(response.dump(), "application/json");

        } catch (const json::exception& e) {
            json error = {
                {"error", {
                    {"code", "INVALID_REQUEST"},
                    {"message", e.what()}
                }}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"error", {
                    {"code", "INTERNAL_ERROR"},
                    {"message", e.what()}
                }}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /api/sessions/:id/history - Get conversation history
    server_.Get(R"(/api/sessions/([^/]+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        AgentSession* session = session_manager_.get_session(session_id);

        if (!session) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        const auto& history = session->agent->get_history();
        json messages = json::array();

        for (const auto& msg : history) {
            json msg_json = {
                {"role", msg.role},
                {"content", msg.content}
            };
            if (!msg.tool_call_id.empty()) {
                msg_json["tool_call_id"] = msg.tool_call_id;
            }
            if (!msg.tool_calls.is_null()) {
                msg_json["tool_calls"] = msg.tool_calls;
            }
            messages.push_back(msg_json);
        }

        json response = {
            {"session_id", session_id},
            {"message_count", history.size()},
            {"messages", messages}
        };

        res.set_content(response.dump(), "application/json");
    });

    // DELETE /api/sessions/:id/history - Clear conversation history
    server_.Delete(R"(/api/sessions/([^/]+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        AgentSession* session = session_manager_.get_session(session_id);

        if (!session) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        session->agent->clear_history();

        json response = {
            {"success", true},
            {"message", "History cleared"}
        };

        res.set_content(response.dump(), "application/json");
    });

    // TODO: Phase 3 - Tool endpoints
    // GET /api/tools/available
    // GET /api/sessions/:id/tools
    // POST /api/sessions/:id/tools
    // DELETE /api/sessions/:id/tools/:name

    // TODO: Phase 4 - Conversation endpoints
    // POST /api/conversations
    // GET /api/conversations
    // GET /api/conversations/:id
}

void ApiServer::start() {
    std::cout << "Loom API server starting on http://localhost:" << port_ << std::endl;
    std::cout << "Health check: http://localhost:" << port_ << "/api/health" << std::endl;
    server_.listen("0.0.0.0", port_);
}

void ApiServer::stop() {
    server_.stop();
}

AgentSessionManager& ApiServer::get_session_manager() {
    return session_manager_;
}
