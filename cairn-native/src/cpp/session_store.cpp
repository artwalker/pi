#include "cairn/session_store.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace cairn {

SessionStore::SessionStore(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
        if (db_) sqlite3_close(db_);
        throw std::runtime_error("failed to open session db: " + err);
    }
    const char* ddl =
        "CREATE TABLE IF NOT EXISTS messages ("
        "  session TEXT NOT NULL,"
        "  idx     INTEGER NOT NULL,"
        "  payload TEXT NOT NULL,"
        "  PRIMARY KEY (session, idx)"
        ");";
    char* errmsg = nullptr;
    if (sqlite3_exec(db_, ddl, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("failed to init schema: " + err);
    }
}

SessionStore::~SessionStore() {
    if (db_) sqlite3_close(db_);
}

void SessionStore::append(const std::string& session, const Json& message) {
    const char* sql =
        "INSERT INTO messages (session, idx, payload) VALUES (?, "
        "(SELECT COALESCE(MAX(idx), -1) + 1 FROM messages WHERE session = ?), ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("prepare failed: ") + sqlite3_errmsg(db_));
    }
    const std::string payload = message.dump();
    sqlite3_bind_text(stmt, 1, session.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, session.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(std::string("insert failed: ") + sqlite3_errmsg(db_));
    }
    sqlite3_finalize(stmt);
}

Json SessionStore::load(const std::string& session) {
    const char* sql = "SELECT payload FROM messages WHERE session = ? ORDER BY idx ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("prepare failed: ") + sqlite3_errmsg(db_));
    }
    sqlite3_bind_text(stmt, 1, session.c_str(), -1, SQLITE_TRANSIENT);

    Json out = Json::array();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            out.push_back(Json::parse(reinterpret_cast<const char*>(text)));
        }
    }
    sqlite3_finalize(stmt);
    return out;
}

}  // namespace cairn
