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
#include <random>

using json = nlohmann::json;

// Authentication macro for cleaner endpoint code
#define REQUIRE_AUTH() \
    std::string client_id; \
    std::optional<std::string> session_id; \
    if (!authenticate_request(req, res, client_id, session_id)) return;

// Helper to format timestamp as ISO 8601
std::string format_timestamp(const std::chrono::time_point<std::chrono::system_clock>& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

ApiServer::ApiServer(int port, std::shared_ptr<SupabaseClient> db, std::shared_ptr<AuthManager> auth)
    : port_(port), db_(db), auth_(auth) {
    configure_cors();
    configure_routes();
}

void ApiServer::configure_cors() {
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
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

bool ApiServer::authenticate_request(const httplib::Request& req,
                                      httplib::Response& res,
                                      std::string& client_id_out,
                                      std::optional<std::string>& session_id_out) {
    // 1. Get Authorization header
    if (!req.has_header("Authorization")) {
        res.status = 401;
        json error = {
            {"error", {
                {"code", "MISSING_AUTH"},
                {"message", "Missing Authorization header"}
            }}
        };
        res.set_content(error.dump(), "application/json");
        return false;
    }

    std::string auth_header = req.get_header_value("Authorization");
    if (auth_header.find("Bearer ") != 0) {
        res.status = 401;
        json error = {
            {"error", {
                {"code", "INVALID_AUTH_FORMAT"},
                {"message", "Authorization must be 'Bearer {api_key}'"}
            }}
        };
        res.set_content(error.dump(), "application/json");
        return false;
    }

    // 2. Extract and hash API key
    std::string api_key = auth_header.substr(7);  // Remove "Bearer "
    std::string key_hash = AuthManager::hash_api_key(api_key);

    // 3. Look up in api_key_aliases table (only authentication method)
    auto alias_opt = db_->authenticateApiKey(key_hash);
    if (!alias_opt) {
        res.status = 401;
        json error = {
            {"error", {
                {"code", "INVALID_API_KEY"},
                {"message", "Invalid or inactive API key"}
            }}
        };
        res.set_content(error.dump(), "application/json");
        return false;
    }

    auto alias = *alias_opt;
    client_id_out = alias.client_id;
    session_id_out = alias.session_id;

    // 4. Set RLS context for this request
    db_->set_client_context(client_id_out);

    return true;
}

void ApiServer::configure_routes() {
    // Health check
    server_.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({"status": "ok"})", "application/json");
    });

    // POST /api/clients - Create new client (PUBLIC - no auth required!)
    server_.Post("/api/clients", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string display_name = body.value("display_name", "");

            // Create client (no API key yet)
            auto client_id_opt = db_->create_client(display_name);
            if (!client_id_opt) {
                json error = {
                    {"error", {
                        {"code", "INTERNAL_ERROR"},
                        {"message", "Failed to create client"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            std::string client_id = *client_id_opt;

            // Generate base API key
            auto base_key_opt = db_->createBaseApiKey(client_id);
            if (!base_key_opt) {
                // Client created but key generation failed - orphaned client
                // Could delete client here or just error
                json error = {
                    {"error", {
                        {"code", "INTERNAL_ERROR"},
                        {"message", "Failed to generate API key"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            json response = {
                {"client_id", client_id},
                {"api_key", *base_key_opt},
                {"message", "Save this API key - it won't be shown again"}
            };
            res.status = 201;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {
                {"error", {
                    {"code", "INVALID_REQUEST"},
                    {"message", std::string("Error creating client: ") + e.what()}
                }}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    });

    // GET /api/clients/me - Get current client info (requires auth)
    server_.Get("/api/clients/me", [this](const httplib::Request& req, httplib::Response& res) {
        REQUIRE_AUTH();

        json response = {
            {"client_id", client_id}
        };
        res.set_content(response.dump(), "application/json");
    });

    // POST /api/sessions - Create new session
    server_.Post("/api/sessions", [this](const httplib::Request& req, httplib::Response& res) {
        REQUIRE_AUTH();

        // Validate that they're using a base key, not a session key
        if (session_id.has_value()) {
            json error = {
                {"error", {
                    {"code", "INVALID_REQUEST"},
                    {"message", "Cannot create session with session key. Use base key."}
                }}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);

            std::string engine = body.value("engine", "chatgpt");
            std::string model = body.value("model", "gpt-3.5-turbo");
            std::string system_prompt = body.value("system_prompt", "");

            // Create session in database
            auto new_session_id_opt = db_->createSession(engine, model, client_id, system_prompt);

            if (!new_session_id_opt) {
                json error = {
                    {"error", {
                        {"code", "INTERNAL_ERROR"},
                        {"message", "Failed to create session"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            std::string new_session_id = *new_session_id_opt;

            // Generate session-specific API key
            auto session_key_opt = db_->createSessionApiKey(client_id, new_session_id);
            if (!session_key_opt) {
                // Session created but key generation failed
                // Delete the session to avoid orphans
                db_->deleteSession(new_session_id, client_id);
                json error = {
                    {"error", {
                        {"code", "INTERNAL_ERROR"},
                        {"message", "Failed to generate session API key"}
                    }}
                };
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }

            json response = {
                {"session_id", new_session_id},
                {"session_api_key", *session_key_opt},
                {"engine", engine},
                {"model", model},
                {"system_prompt", system_prompt},
                {"created_at", format_timestamp(std::chrono::system_clock::now())}
            };

            res.status = 201;
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
        REQUIRE_AUTH();
        auto sessions = db_->listClientSessions(client_id);

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
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];

        auto session_opt = db_->getSession(requested_session_id, client_id);

        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        auto session = session_opt.value();
        auto history = db_->getConversationHistory(requested_session_id, client_id);

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
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];
        bool destroyed = db_->deleteSession(requested_session_id, client_id);

        if (!destroyed) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
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

    // OpenAI-compatible endpoints
    server_.Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& res) {
        handle_chat_completion(req, res);
    });

    server_.Get("/v1/models", [this](const httplib::Request& req, httplib::Response& res) {
        handle_list_models(req, res);
    });

    // GET /api/sessions/:id/history - Get conversation history
    server_.Get(R"(/api/sessions/([^/]+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];

        // Verify session exists
        auto session_opt = db_->getSession(requested_session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        // Load history from database
        auto history = db_->getConversationHistory(requested_session_id, client_id);
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
            {"session_id", requested_session_id},
            {"message_count", history.size()},
            {"messages", messages}
        };

        res.set_content(response.dump(), "application/json");
    });

    // DELETE /api/sessions/:id/history - Clear conversation history
    server_.Delete(R"(/api/sessions/([^/]+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];

        // Verify session exists
        auto session_opt = db_->getSession(requested_session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        // Clear history in database
        db_->clearHistory(requested_session_id, client_id);

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
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];

        // Verify session exists
        auto session_opt = db_->getSession(requested_session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        // Load tool names from database
        auto tool_names = db_->getSessionTools(requested_session_id, client_id);
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
            {"session_id", requested_session_id},
            {"tools", tools},
            {"tool_count", tools.size()}
        };

        res.set_content(response.dump(), "application/json");
    });

    // POST /api/sessions/:id/tools - Register tools to session
    server_.Post(R"(/api/sessions/([^/]+)/tools)", [this](const httplib::Request& req, httplib::Response& res) {
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];

        // Verify session exists
        auto session_opt = db_->getSession(requested_session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
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
                db_->registerTools(requested_session_id, client_id, registered);
            }

            json response = {
                {"session_id", requested_session_id},
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
        REQUIRE_AUTH();
        std::string requested_session_id = req.matches[1];
        std::string tool_name = req.matches[2];

        // Verify session exists
        auto session_opt = db_->getSession(requested_session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session with ID '" + requested_session_id + "' not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }

        // Unregister from database
        db_->unregisterTools(requested_session_id, client_id, {tool_name});

        json response = {
            {"session_id", requested_session_id},
            {"removed_tool", tool_name},
            {"success", true}
        };

        res.set_content(response.dump(), "application/json");
    });
}

// ============================================
// OpenAI-Compatible Endpoints
// ============================================

// Exact model lookup table
const std::unordered_map<std::string, std::string> ApiServer::MODEL_ENGINE_MAP = {
    // OpenAI models
    {"gpt-4", "chatgpt"},
    {"gpt-4-turbo", "chatgpt"},
    {"gpt-4-turbo-preview", "chatgpt"},
    {"gpt-4o", "chatgpt"},
    {"gpt-4o-mini", "chatgpt"},
    {"gpt-3.5-turbo", "chatgpt"},
    {"gpt-3.5-turbo-16k", "chatgpt"},
    {"o1-preview", "chatgpt"},
    {"o1-mini", "chatgpt"},

    // Gemini models
    {"gemini-pro", "gemini"},
    {"gemini-1.5-pro", "gemini"},
    {"gemini-1.5-flash", "gemini"},
    {"gemini-ultra", "gemini"},

    // Ollama models (common ones)
    {"llama3", "ollama"},
    {"llama3:8b", "ollama"},
    {"llama3:70b", "ollama"},
    {"llama2", "ollama"},
    {"llama2:7b", "ollama"},
    {"llama2:13b", "ollama"},
    {"mistral", "ollama"},
    {"mistral:7b", "ollama"},
    {"mixtral", "ollama"},
    {"mixtral:8x7b", "ollama"},
    {"phi", "ollama"},
    {"phi:2.7b", "ollama"},
    {"codellama", "ollama"},
    {"codellama:7b", "ollama"},
    {"codellama:13b", "ollama"},
    {"qwen", "ollama"},
    {"qwen:7b", "ollama"}
};

std::string ApiServer::infer_engine_from_model(const std::string& model) {
    // Exact match lookup
    auto it = MODEL_ENGINE_MAP.find(model);

    if (it != MODEL_ENGINE_MAP.end()) {
        return it->second;
    }

    // Model not found - return helpful error
    throw std::invalid_argument(
        "Unknown model: '" + model + "'. " +
        "Supported models: gpt-4, gpt-4-turbo, gpt-3.5-turbo, o1-preview, " +
        "gemini-pro, gemini-1.5-pro, llama3, mistral, phi, codellama, etc. " +
        "See /v1/models endpoint for full list."
    );
}

std::string ApiServer::generate_random_id() {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);

    std::string id;
    for (int i = 0; i < 29; i++) {
        id += chars[dis(gen)];
    }
    return id;
}

json ApiServer::build_openai_response(const ToolResult& result, const std::string& model) {
    // If there's an error, log it and return error response
    if (!result.is_success()) {
        std::string error_msg = result.get_error().value_or("Unknown error");
        std::cerr << "Error in chat completion: " << error_msg << std::endl;

        return {
            {"error", {
                {"code", "COMPLETION_ERROR"},
                {"message", error_msg}
            }}
        };
    }

    return {
        {"id", "chatcmpl-" + generate_random_id()},
        {"object", "chat.completion"},
        {"created", std::time(nullptr)},
        {"model", model},
        {"choices", json::array({
            {
                {"index", 0},
                {"message", {
                    {"role", "assistant"},
                    {"content", result.get_message()}
                }},
                {"finish_reason", "stop"}
            }
        })},
        {"usage", {
            {"prompt_tokens", 0},
            {"completion_tokens", 0},
            {"total_tokens", 0}
        }}
    };
}

void ApiServer::handle_chat_completion(const httplib::Request& req, httplib::Response& res) {
    // Authenticate
    std::string client_id;
    std::optional<std::string> session_id;
    if (!authenticate_request(req, res, client_id, session_id)) {
        return;
    }

    // Parse request
    json request_body;
    try {
        request_body = json::parse(req.body);
    } catch (const json::exception& e) {
        json error = {
            {"error", {
                {"code", "INVALID_REQUEST"},
                {"message", "Invalid JSON"}
            }}
        };
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
    }

    if (!request_body.contains("model") || !request_body.contains("messages")) {
        json error = {
            {"error", {
                {"code", "INVALID_REQUEST"},
                {"message", "Missing required fields: model, messages"}
            }}
        };
        res.status = 400;
        res.set_content(error.dump(), "application/json");
        return;
    }

    // Route based on key type
    if (session_id.has_value()) {
        // Session key: use session history
        handle_session_completion(res, client_id, *session_id, request_body);
    } else {
        // Base key: stateless
        handle_stateless_completion(res, client_id, request_body);
    }
}

void ApiServer::handle_session_completion(
    httplib::Response& res,
    const std::string& client_id,
    const std::string& session_id,
    const json& request_body
) {
    try {
        // Load session
        auto session_opt = db_->getSession(session_id, client_id);
        if (!session_opt) {
            json error = {
                {"error", {
                    {"code", "SESSION_NOT_FOUND"},
                    {"message", "Session not found"}
                }}
            };
            res.status = 404;
            res.set_content(error.dump(), "application/json");
            return;
        }
        auto session = *session_opt;

        // Load history
        auto history = db_->getConversationHistory(session_id, client_id);

        // Create agent
        auto engine = create_engine(session.engine_type);
        Agent agent(engine, session.model, false);

        // Set system prompt if exists
        if (!session.system_prompt.empty()) {
            agent.add_message("system", session.system_prompt);
        }

        // Load existing messages
        for (const auto& msg : history) {
            agent.add_message(msg.role, msg.content);
        }

        // Smart deduplication - only check tail of history
        size_t history_size = history.size();
        auto request_messages = request_body["messages"];
        size_t request_size = request_messages.size();

        std::vector<ChatMessage> new_messages;

        // If request has more messages than history, new ones are at the end
        if (request_size > history_size) {
            for (size_t i = history_size; i < request_size; i++) {
                std::string role = request_messages[i]["role"];
                std::string content = request_messages[i]["content"];

                agent.add_message(role, content);
                new_messages.push_back(ChatMessage(role, content));
            }
        }

        // Execute
        std::map<std::string, std::any> params;
        if (request_body.contains("temperature")) {
            params["temperature"] = request_body["temperature"].get<double>();
        } else {
            params["temperature"] = 1.0;
        }
        if (request_body.contains("max_tokens")) {
            params["max_tokens"] = request_body["max_tokens"].get<int>();
        } else {
            params["max_tokens"] = -1;
        }

        auto result = agent.execute_safe(params);

        // Save new messages
        for (const auto& msg : new_messages) {
            db_->add_message(session_id, client_id, msg.role, msg.content);
        }

        // Save assistant response
        if (result.is_success()) {
            db_->add_message(session_id, client_id, "assistant", result.get_message());
        }

        // Build OpenAI response
        json openai_response = build_openai_response(result, session.model);

        res.set_content(openai_response.dump(), "application/json");
        res.status = 200;
    } catch (const std::exception& e) {
        json error = {
            {"error", {
                {"code", "INTERNAL_ERROR"},
                {"message", std::string("Error processing chat completion: ") + e.what()}
            }}
        };
        res.status = 500;
        res.set_content(error.dump(), "application/json");
    }
}

void ApiServer::handle_stateless_completion(
    httplib::Response& res,
    const std::string& client_id,
    const json& request_body
) {
    try {
        std::string model = request_body["model"];

        // Validate model and infer engine
        std::string engine_type;
        try {
            engine_type = infer_engine_from_model(model);
        } catch (const std::invalid_argument& e) {
            json error = {
                {"error", {
                    {"code", "INVALID_MODEL"},
                    {"message", e.what()}
                }}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");
            return;
        }

        // Create agent
        auto engine = create_engine(engine_type);
        Agent agent(engine, model, false);

        // Load all messages
        for (const auto& msg : request_body["messages"]) {
            agent.add_message(msg["role"], msg["content"]);
        }

        // Execute
        std::map<std::string, std::any> params;
        if (request_body.contains("temperature")) {
            params["temperature"] = request_body["temperature"].get<double>();
        } else {
            params["temperature"] = 1.0;
        }
        if (request_body.contains("max_tokens")) {
            params["max_tokens"] = request_body["max_tokens"].get<int>();
        } else {
            params["max_tokens"] = -1;
        }

        auto result = agent.execute_safe(params);

        // Build OpenAI response
        json openai_response = build_openai_response(result, model);

        res.set_content(openai_response.dump(), "application/json");
        res.status = 200;
    } catch (const std::exception& e) {
        json error = {
            {"error", {
                {"code", "INTERNAL_ERROR"},
                {"message", std::string("Error processing chat completion: ") + e.what()}
            }}
        };
        res.status = 500;
        res.set_content(error.dump(), "application/json");
    }
}

void ApiServer::handle_list_models(const httplib::Request& req, httplib::Response& res) {
    // Build list of all supported models from the map
    json models_array = json::array();

    for (const auto& [model_name, engine] : MODEL_ENGINE_MAP) {
        models_array.push_back({
            {"id", model_name},
            {"object", "model"},
            {"created", 1677649963},
            {"owned_by", engine}
        });
    }

    json response = {
        {"object", "list"},
        {"data", models_array}
    };

    res.set_content(response.dump(), "application/json");
    res.status = 200;
}

void ApiServer::start() {
    std::cout << "Loom API server starting on http://localhost:" << port_ << std::endl;
    std::cout << "Health check: http://localhost:" << port_ << "/api/health" << std::endl;
    std::cout << "Multi-tenant mode: Authentication required for all endpoints" << std::endl;
    std::cout << "Signup endpoint: POST http://localhost:" << port_ << "/api/clients" << std::endl;
    server_.listen("0.0.0.0", port_);
}

void ApiServer::stop() {
    server_.stop();
}
