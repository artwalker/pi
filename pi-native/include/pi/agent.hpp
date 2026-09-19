#pragma once
// C++ body: the agent loop.
//
// Owns the conversation, drives the model, dispatches tool calls, and persists
// state. This is the "pi-agent-core" analogue.

#include <functional>
#include <string>

#include "pi/llm_client.hpp"
#include "pi/session_store.hpp"
#include "pi/tool.hpp"

namespace pi {

class Agent {
public:
    Agent(LlmClient& client, ToolRegistry& tools, SessionStore& store,
          std::string session, std::string system_prompt);

    // Run one user turn to completion (resolving any tool calls), returning the
    // final assistant text. `on_token` streams assistant content deltas.
    std::string run(const std::string& user_input,
                    const std::function<void(const std::string&)>& on_token,
                    const std::function<void(const std::string&, const std::string&)>& on_tool);

    const Json& history() const { return history_; }

private:
    LlmClient&    client_;
    ToolRegistry& tools_;
    SessionStore& store_;
    std::string   session_;
    Json          history_;

    void record(const Json& message);
};

}  // namespace pi
