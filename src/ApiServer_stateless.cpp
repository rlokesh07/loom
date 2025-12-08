#include "../include/ApiServer_stateless.hpp"
#include "AvailableToolsRegistry.hpp"
#include "ChatGPTCompletionEngine.hpp"
#include "OllamaCompletionEngine.hpp"
#include "GeminiCompletionEngine.hpp"
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

ApiServer::ApiServer(int port, std::shared_ptr<SupabaseClient> db, const std::string& client_id)
    : port_(port), db_(db), default_client_id_(client_id) {
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

std::shared_ptr<ToolRegistry> ApiServer::build_tool_registry(const std::vector<std::string>& tool_names) {
    auto registry = std::make_shared<ToolRegistry>();

    for (const auto& name : tool_names) {
        if (AvailableToolsRegistry::is_available(name)) {
            auto tool = AvailableToolsRegistry::create_tool(name);
            registry->register_tool(tool);
        }
    }

    return registry;
}

std::shared_ptr<CompletionEngine> ApiServer::create_engine(const std::string& engine_type) {
    if (engine_type == "chatgpt") {
        return std::make_shared<ChatGPTCompletionEngine>();
    } else if (engine_type == "ollama") {
        return std::make_shared<OllamaCompletionEngine>();
    } else if (engine_type == "gemini") {
        return std::make_shared<GeminiCompletionEngine>();
    }

    throw std::invalid_argument("Unknown engine type: " + engine_type);
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
            std::string system_prompt = body.value("system_prompt", "");

            // Create session in database
            auto session_id_opt = db_->createSession(engine, model, default_client_id_, system_prompt);

            if (!session_id_opt) {
                json error = {
                    {"error", {
                        {"code", "SESSION_CREATION_FAILED"},
                        {"message", "Failed to create session in database"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            std::string session_id = session_id_opt.value();

            // Load session metadata to return details
            auto session_opt = db_->getSession(session_id, default_client_id_);
            if (!session_opt) {
                json error = {
                    {"error", {
                        {"code", "SESSION_NOT_FOUND"},
                        {"message", "Session created but could not be retrieved"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            auto session = session_opt.value();

            json response = {
                {"session_id", session.id},
                {"engine", session.engine_type},
                {"model", session.model},
                {"system_prompt", session.system_prompt},
                {"created_at", format_timestamp(session.created_at)}
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
        auto sessions = db_->listClientSessions(default_client_id_);

        json sessions_array = json::array();
        for (const auto& session : sessions) {
            sessions_array.push_back({
                {"session_id", session.id},
                {"engine", session.engine_type},
                {"model", session.model},
                {"system_prompt", session.system_prompt},
                {"created_at", format_timestamp(session.created_at)},
                {"last_accessed_at", format_timestamp(session.last_accessed_at)}
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

        auto session_opt = db_->getSession(session_id, default_client_id_);

        if (!session_opt) {
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

        auto session = session_opt.value();
        auto history = db_->getConversationHistory(session_id, default_client_id_);

        json response = {
            {"session_id", session.id},
            {"engine", session.engine_type},
            {"model", session.model},
            {"system_prompt", session.system_prompt},
            {"created_at", format_timestamp(session.created_at)},
            {"last_accessed_at", format_timestamp(session.last_accessed_at)},
            {"history_size", history.size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // DELETE /api/sessions/:id - Delete session
    server_.Delete(R"(/api/sessions/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];
        bool destroyed = db_->deleteSession(session_id, default_client_id_);

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

    // POST /api/sessions/:id/chat - Send message (KEY STATELESS ENDPOINT)
    server_.Post(R"(/api/sessions/([^/]+)/chat)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];

        try {
            // 1. Load session metadata from database
            auto session_opt = db_->getSession(session_id, default_client_id_);
            if (!session_opt) {
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
            auto session = session_opt.value();

            // 2. Load conversation history from database
            auto history = db_->getConversationHistory(session_id, default_client_id_);

            // 3. Load tool names from database
            auto tool_names = db_->getSessionTools(session_id, default_client_id_);

            // 4. Build tool registry from names
            auto registry = build_tool_registry(tool_names);

            // 5. Create completion engine
            auto engine = create_engine(session.engine_type);

            // 6. Create FRESH agent (stateless!)
            Agent agent(engine, session.model, false);
            agent.set_tool_registry(registry);

            // 7. Load history into agent
            for (const auto& msg : history) {
                agent.add_message(msg.role, msg.content);
            }

            // 8. Add system prompt if exists
            if (!session.system_prompt.empty() && history.empty()) {
                agent.add_message("system", session.system_prompt);
            }

            // 9. Parse request
            json body = json::parse(req.body);
            std::string user_message = body.at("message");

            std::map<std::string, std::any> params;
            params["message"] = user_message;

            if (body.contains("temperature")) {
                params["temperature"] = body["temperature"].get<double>();
            }
            if (body.contains("max_tokens")) {
                params["max_tokens"] = body["max_tokens"].get<int>();
            }

            // 10. Execute agent
            auto result = agent.execute_safe(params);

            // 11. Save new messages to database
            db_->add_message(session_id, default_client_id_, "user", user_message);
            if (result.is_success()) {
                std::string assistant_response = result.get_message();
                db_->add_message(session_id, default_client_id_, "assistant", assistant_response);
            }

            // 12. Build response
            json response = {
                {"success", result.is_success()},
                {"message", result.get_message()}
            };

            if (!result.is_success() && result.get_error()) {
                response["error"] = *result.get_error();
            }

            // Include tool calls if any were made
            const auto& tool_calls = agent.get_last_tool_calls();
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

        // Verify session exists
        auto session_opt = db_->getSession(session_id, default_client_id_);
        if (!session_opt) {
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

        // Load history from database
        auto history = db_->getConversationHistory(session_id, default_client_id_);
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

        // Verify session exists
        auto session_opt = db_->getSession(session_id, default_client_id_);
        if (!session_opt) {
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

        // Clear history in database
        db_->clearHistory(session_id, default_client_id_);

        json response = {
            {"success", true},
            {"message", "History cleared"}
        };

        res.set_content(response.dump(), "application/json");
    });

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

        // Verify session exists
        auto session_opt = db_->getSession(session_id, default_client_id_);
        if (!session_opt) {
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

        // Load tool names from database
        auto tool_names = db_->getSessionTools(session_id, default_client_id_);
        json tools = json::array();

        for (const auto& name : tool_names) {
            if (AvailableToolsRegistry::is_available(name)) {
                auto tool = AvailableToolsRegistry::create_tool(name);
                tools.push_back({
                    {"name", name},
                    {"description", tool->get_description()}
                });
            } else {
                json unknown_tool = {
                    {"name", name},
                    {"description", "Unknown tool"}
                };
                tools.push_back(unknown_tool);
            }
        }

        json response = {
            {"session_id", session_id},
            {"tools", tools},
            {"tool_count", tools.size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // POST /api/sessions/:id/tools - Register tools to session
    server_.Post(R"(/api/sessions/([^/]+)/tools)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string session_id = req.matches[1];

        // Verify session exists
        auto session_opt = db_->getSession(session_id, default_client_id_);
        if (!session_opt) {
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

            std::vector<std::string> registered;
            std::vector<std::string> failed;

            for (const auto& name : tool_names) {
                if (!AvailableToolsRegistry::is_available(name)) {
                    failed.push_back(name);
                    continue;
                }
                registered.push_back(name);
            }

            // Register tools in database
            if (!registered.empty()) {
                db_->registerTools(session_id, default_client_id_, registered);
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

        // Verify session exists
        auto session_opt = db_->getSession(session_id, default_client_id_);
        if (!session_opt) {
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

        // Unregister from database
        db_->unregisterTools(session_id, default_client_id_, {tool_name});

        json response = {
            {"session_id", session_id},
            {"removed_tool", tool_name},
            {"success", true}
        };

        res.set_content(response.dump(), "application/json");
    });
}

void ApiServer::start() {
    std::cout << "Loom API server starting on http://localhost:" << port_ << std::endl;
    std::cout << "Health check: http://localhost:" << port_ << "/api/health" << std::endl;
    std::cout << "Client ID: " << default_client_id_ << std::endl;
    server_.listen("0.0.0.0", port_);
}

void ApiServer::stop() {
    server_.stop();
}
