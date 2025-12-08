#include "SupabaseClient.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <iomanip>

using json = nlohmann::json;

// Helper function for CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper to parse ISO 8601 timestamp to time_point
static std::chrono::time_point<std::chrono::system_clock> parseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Private helper function to make HTTP requests to Supabase REST API
std::string SupabaseClient::makeRequest(const std::string& method, const std::string& endpoint,
                                        const json& body, const std::string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response_string;
    std::string url = baseURL + endpoint;
    if (!query.empty()) {
        url += "?" + query;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("apikey: " + apiKey).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Prefer: return=representation");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    // Keep body_str alive for the entire request
    std::string body_str;

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        body_str = body.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body_str.length());
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        body_str = body.dump();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body_str.length());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL request failed: ") + curl_easy_strerror(res));
    }

    return response_string;
}

// ============================================
// Session operations
// ============================================

std::optional<std::string> SupabaseClient::createSession(const std::string& engine_type,
                                                          const std::string& model,
                                                          const std::string& clientID,
                                                          const std::string& system_prompt) {
    try {
        // Let database defaults handle conversation_history and tool_names
        json body = {
            {"client_id", clientID},
            {"engine_type", engine_type},
            {"model", model},
            {"system_prompt", system_prompt}
        };

        std::string response = makeRequest("POST", "/rest/v1/sessions", body);

        if (response.empty()) {
            return std::nullopt;
        }

        json response_json = json::parse(response);

        if (response_json.is_array() && !response_json.empty()) {
            return response_json[0]["session_id"].get<std::string>();
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error creating session: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<SessionMetadata> SupabaseClient::getSession(const std::string& sessionID,
                                                           const std::string& clientID) {
    try {
        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID + "&select=*";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        if (response.empty()) {
            return std::nullopt;
        }

        json response_json = json::parse(response);
        if (response_json.is_array() && !response_json.empty()) {
            json session = response_json[0];

            SessionMetadata metadata;
            metadata.id = session["session_id"].get<std::string>();
            metadata.client_id = session["client_id"].get<std::string>();
            metadata.engine_type = session["engine_type"].get<std::string>();
            metadata.model = session["model"].get<std::string>();
            metadata.system_prompt = session.value("system_prompt", "");
            metadata.created_at = parseTimestamp(session["created_at"].get<std::string>());
            metadata.last_accessed_at = parseTimestamp(session["last_accessed_at"].get<std::string>());

            return metadata;
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error getting session: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool SupabaseClient::deleteSession(const std::string& sessionID, const std::string& clientID) {
    try {
        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID;
        makeRequest("DELETE", "/rest/v1/sessions", json::object(), query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting session: " << e.what() << std::endl;
        return false;
    }
}

std::vector<SessionMetadata> SupabaseClient::listClientSessions(const std::string& client_id) {
    std::vector<SessionMetadata> sessions;

    try {
        std::string query = "client_id=eq." + client_id + "&select=*&order=created_at.desc";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);

            if (response_json.is_array()) {
                for (const auto& session : response_json) {
                    SessionMetadata metadata;
                    metadata.id = session["session_id"].get<std::string>();
                    metadata.client_id = session["client_id"].get<std::string>();
                    metadata.engine_type = session["engine_type"].get<std::string>();
                    metadata.model = session["model"].get<std::string>();
                    metadata.system_prompt = session.value("system_prompt", "");
                    metadata.created_at = parseTimestamp(session["created_at"].get<std::string>());
                    metadata.last_accessed_at = parseTimestamp(session["last_accessed_at"].get<std::string>());

                    sessions.push_back(metadata);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing sessions: " << e.what() << std::endl;
    }

    return sessions;
}

// ============================================
// Conversation operations
// ============================================

void SupabaseClient::add_message(const std::string& sessionid, const std::string& clientid,
                                  const std::string& role, const std::string& content) {
    try {
        // First, get current conversation history
        std::string query = "session_id=eq." + sessionid + "&client_id=eq." + clientid + "&select=conversation_history";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        json conversation_history = json::array();
        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                conversation_history = response_json[0]["conversation_history"];
            }
        }

        // Append new message
        json new_message = {
            {"role", role},
            {"content", content}
        };
        conversation_history.push_back(new_message);

        // Update in database
        json update_body = {
            {"conversation_history", conversation_history}
        };

        query = "session_id=eq." + sessionid + "&client_id=eq." + clientid;
        makeRequest("PATCH", "/rest/v1/sessions", update_body, query);

    } catch (const std::exception& e) {
        std::cerr << "Error adding message: " << e.what() << std::endl;
    }
}

std::vector<ChatMessage> SupabaseClient::getConversationHistory(const std::string& sessionid,
                                                                 const std::string& clientid) {
    std::vector<ChatMessage> history;

    try {
        std::string query = "session_id=eq." + sessionid + "&client_id=eq." + clientid + "&select=conversation_history";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                json conversation = response_json[0]["conversation_history"];

                for (const auto& msg : conversation) {
                    std::string role = msg["role"].get<std::string>();
                    std::string content = msg["content"].get<std::string>();
                    ChatMessage chat_msg(role, content);
                    history.push_back(chat_msg);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting conversation history: " << e.what() << std::endl;
    }

    return history;
}

bool SupabaseClient::clearHistory(const std::string& sessionID, const std::string& clientID) {
    try {
        json update_body = {
            {"conversation_history", json::array()}
        };

        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID;
        makeRequest("PATCH", "/rest/v1/sessions", update_body, query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error clearing history: " << e.what() << std::endl;
        return false;
    }
}

// ============================================
// Tool operations
// ============================================

bool SupabaseClient::registerTools(const std::string& sessionID, const std::string& clientID,
                                    const std::vector<std::string>& tool_names) {
    try {
        // Get current tool names
        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID + "&select=tool_names";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        json current_tools = json::array();
        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                current_tools = response_json[0]["tool_names"];
            }
        }

        // Merge new tools (avoid duplicates)
        for (const auto& tool : tool_names) {
            bool exists = false;
            for (const auto& existing : current_tools) {
                if (existing.get<std::string>() == tool) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                current_tools.push_back(tool);
            }
        }

        // Update database
        json update_body = {
            {"tool_names", current_tools}
        };

        query = "session_id=eq." + sessionID + "&client_id=eq." + clientID;
        makeRequest("PATCH", "/rest/v1/sessions", update_body, query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error registering tools: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> SupabaseClient::getSessionTools(const std::string& sessionID,
                                                          const std::string& clientID) {
    std::vector<std::string> tools;

    try {
        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID + "&select=tool_names";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                json tool_names = response_json[0]["tool_names"];

                for (const auto& tool : tool_names) {
                    tools.push_back(tool.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting session tools: " << e.what() << std::endl;
    }

    return tools;
}

bool SupabaseClient::unregisterTools(const std::string& sessionID, const std::string& clientID,
                                      const std::vector<std::string>& tool_names) {
    try {
        // Get current tool names
        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID + "&select=tool_names";
        std::string response = makeRequest("GET", "/rest/v1/sessions", json::object(), query);

        json current_tools = json::array();
        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                current_tools = response_json[0]["tool_names"];
            }
        }

        // Remove specified tools
        json updated_tools = json::array();
        for (const auto& tool : current_tools) {
            std::string tool_str = tool.get<std::string>();
            bool should_remove = false;

            for (const auto& remove_tool : tool_names) {
                if (tool_str == remove_tool) {
                    should_remove = true;
                    break;
                }
            }

            if (!should_remove) {
                updated_tools.push_back(tool_str);
            }
        }

        // Update database
        json update_body = {
            {"tool_names", updated_tools}
        };

        query = "session_id=eq." + sessionID + "&client_id=eq." + clientID;
        makeRequest("PATCH", "/rest/v1/sessions", update_body, query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error unregistering tools: " << e.what() << std::endl;
        return false;
    }
}

bool SupabaseClient::clearTools(const std::string& sessionID, const std::string& clientID) {
    try {
        json update_body = {
            {"tool_names", json::array()}
        };

        std::string query = "session_id=eq." + sessionID + "&client_id=eq." + clientID;
        makeRequest("PATCH", "/rest/v1/sessions", update_body, query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error clearing tools: " << e.what() << std::endl;
        return false;
    }
}

// ============================================
// Child session operations
// ============================================

bool SupabaseClient::registerChildSession(const std::string& parentSessionID, const std::string& toolName,
                                           const std::string& childSessionID, const std::string& clientID) {
    try {
        json body = {
            {"parent_session_id", parentSessionID},
            {"tool_name", toolName},
            {"child_session_id", childSessionID}
        };

        makeRequest("POST", "/rest/v1/child_sessions", body);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error registering child session: " << e.what() << std::endl;
        return false;
    }
}

std::optional<std::string> SupabaseClient::getChildSession(const std::string& parentSessionID,
                                                            const std::string& toolName,
                                                            const std::string& clientID) {
    try {
        std::string query = "parent_session_id=eq." + parentSessionID + "&tool_name=eq." + toolName + "&select=child_session_id";
        std::string response = makeRequest("GET", "/rest/v1/child_sessions", json::object(), query);

        if (!response.empty()) {
            json response_json = json::parse(response);
            if (response_json.is_array() && !response_json.empty()) {
                return response_json[0]["child_session_id"].get<std::string>();
            }
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Error getting child session: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool SupabaseClient::unregisterChildSession(const std::string& parentSessionID, const std::string& toolName,
                                             const std::string& clientID) {
    try {
        std::string query = "parent_session_id=eq." + parentSessionID + "&tool_name=eq." + toolName;
        makeRequest("DELETE", "/rest/v1/child_sessions", json::object(), query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error unregistering child session: " << e.what() << std::endl;
        return false;
    }
}
