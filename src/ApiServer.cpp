#include "../include/ApiServer.hpp"
#include "AvailableToolsRegistry.hpp"
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

            // Include tool calls if any were made
            const auto& tool_calls = session->agent->get_last_tool_calls();
            if (!tool_calls.empty()) {
                json tool_calls_json = json::array();
                for (const auto& tc : tool_calls) {
                    tool_calls_json.push_back({
                        {"tool_name", tc.tool_name},
                        {"arguments", json::parse(tc.arguments)},
                        {"result", tc.result},
                        {"success", tc.success}
                    });
                }
                response["tool_calls"] = tool_calls_json;
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

    // Phase 3 - Tool endpoints

    // GET /api/tools/available - List all available tools
    server_.Get("/api/tools/available", [](const httplib::Request& req, httplib::Response& res) {
        json tools = json::array();
        for (const auto& info : AvailableToolsRegistry::list_available()) {
            tools.push_back({
                {"name", info.name},
                {"description", info.description}
            });
        }

        json response = {{"tools", tools}};
        res.set_content(response.dump(), "application/json");
    });

    // GET /api/sessions/:id/tools - List tools registered to session
    server_.Get(R"(/api/sessions/([^/]+)/tools)", [this](const httplib::Request& req, httplib::Response& res) {
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

        auto registry = session->agent->get_tool_registry();
        json tools = json::array();

        if (registry) {
            for (const auto& name : registry->list_tools()) {
                auto tool = registry->get_tool(name);
                tools.push_back({
                    {"name", name},
                    {"description", tool->get_description()}
                });
            }
        }

        json response = {
            {"session_id", session_id},
            {"tools", tools},
            {"tool_count", tools.size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // POST /api/sessions/:id/tools/agent - Register another session's agent as a tool
    // NOTE: Must be registered before /tools to avoid route conflict
    server_.Post(R"(/api/sessions/([^/]+)/tools/agent)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string target_session_id = req.matches[1];
        AgentSession* target_session = session_manager_.get_session(target_session_id);

        if (!target_session) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Target session with ID '" + target_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);
            std::string source_session_id = body.at("source_session_id").get<std::string>();

            // Prevent self-registration
            if (target_session_id == source_session_id) {
                json error = {
                    {"error", {
                        {"code", "SELF_REGISTRATION"},
                        {"message", "Cannot register a session as its own tool"}
                    }}
                };
                res.status = 400;
                res.set_content(error.dump(), "application/json");
                return;
            }

            AgentSession* source_session = session_manager_.get_session(source_session_id);
            if (!source_session) {
                json error = {
                    {"error", {
                        {"code", "SESSION_NOT_FOUND"},
                        {"message", "Source session with ID '" + source_session_id + "' not found"}
                    }}
                };
                res.status = 404;
                res.set_content(error.dump(), "application/json");
                return;
            }

            // Get source agent
            auto agent = source_session->agent;

            // Optionally update name and description
            std::string tool_name = agent->get_name();
            if (body.contains("tool_name")) {
                tool_name = body["tool_name"].get<std::string>();
                agent->set_name(tool_name);
            }
            if (body.contains("tool_description")) {
                agent->set_description(body["tool_description"].get<std::string>());
            }

            // Register into target's registry
            auto registry = target_session->agent->get_tool_registry();

            // Check if tool name already exists
            if (registry->has_tool(tool_name)) {
                json error = {
                    {"error", {
                        {"code", "TOOL_ALREADY_EXISTS"},
                        {"message", "Tool with name '" + tool_name + "' already registered"}
                    }}
                };
                res.status = 409;
                res.set_content(error.dump(), "application/json");
                return;
            }

            registry->register_tool(agent);

            json response = {
                {"session_id", target_session_id},
                {"registered_agent_tool", tool_name},
                {"source_session_id", source_session_id},
                {"success", true}
            };

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

    // POST /api/sessions/:id/tools - Register tools to session
    server_.Post(R"(/api/sessions/([^/]+)/tools)", [this](const httplib::Request& req, httplib::Response& res) {
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
            std::vector<std::string> tool_names = body.at("tools").get<std::vector<std::string>>();

            auto registry = session->agent->get_tool_registry();
            std::vector<std::string> registered;
            std::vector<std::string> failed;

            for (const auto& name : tool_names) {
                if (!AvailableToolsRegistry::is_available(name)) {
                    failed.push_back(name);
                    continue;
                }

                if (registry->has_tool(name)) {
                    registered.push_back(name);
                    continue;
                }

                auto tool = AvailableToolsRegistry::create_tool(name);
                registry->register_tool(tool);
                registered.push_back(name);
            }

            json response = {
                {"session_id", session_id},
                {"registered_tools", registered},
                {"tool_count", registered.size()}
            };

            if (!failed.empty()) {
                response["failed_tools"] = failed;
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

    // DELETE /api/sessions/:id/tools/:name - Unregister tool from session
    server_.Delete(R"(/api/sessions/([^/]+)/tools/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        std::string tool_name = req.matches[2];

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

        auto registry = session->agent->get_tool_registry();

        if (!registry->has_tool(tool_name)) {
            json error = {
                {"error", {
                    {"code", "TOOL_NOT_FOUND"},
                    {"message", "Tool '" + tool_name + "' not registered in session"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        registry->unregister_tool(tool_name);

        json response = {
            {"session_id", session_id},
            {"removed_tool", tool_name},
            {"success", true}
        };

        res.set_content(response.dump(), "application/json");
    });

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
