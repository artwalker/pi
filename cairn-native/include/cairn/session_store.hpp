#pragma once
// C++ body: durable session store.
//
// RAII wrapper over SQLite (a C library). Persists the message history so a
// session survives process restarts, mirroring pi-durable / session-backends.

#include <string>

#include <nlohmann/json.hpp>

struct sqlite3;

namespace cairn {

using Json = nlohmann::json;

class SessionStore {
public:
    explicit SessionStore(const std::string& db_path);
    ~SessionStore();

    SessionStore(const SessionStore&) = delete;
    SessionStore& operator=(const SessionStore&) = delete;

    // Append one OpenAI-format message to the session.
    void append(const std::string& session, const Json& message);

    // Load the full ordered message history for a session.
    Json load(const std::string& session);

private:
    sqlite3* db_ = nullptr;
};

}  // namespace cairn
