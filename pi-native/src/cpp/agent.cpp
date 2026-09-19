#include "pi/agent.hpp"

namespace pi {

Agent::Agent(LlmClient& client, ToolRegistry& tools, SessionStore& store,
             std::string session, std::string system_prompt)
    : client_(client), tools_(tools), store_(store), session_(std::move(session)) {
    history_ = store_.load(session_);
    if (history_.empty() && !system_prompt.empty()) {
        record({{"role", "system"}, {"content", system_prompt}});
    }
}

void Agent::record(const Json& message) {
    history_.push_back(message);
    store_.append(session_, message);
}

std::string Agent::run(
    const std::string& user_input,
    const std::function<void(const std::string&)>& on_token,
    const std::function<void(const std::string&, const std::string&)>& on_tool) {
    record({{"role", "user"}, {"content", user_input}});

    // Bound the tool-resolution loop to avoid runaway back-and-forth.
    for (int step = 0; step < 8; ++step) {
        AssistantTurn turn = client_.chat(history_, tools_, on_token);

        Json assistant = {{"role", "assistant"}, {"content", turn.content}};
        if (!turn.tool_calls.empty()) {
            Json calls = Json::array();
            for (const auto& tc : turn.tool_calls) {
                calls.push_back({
                    {"id", tc.id},
                    {"type", "function"},
                    {"function", {{"name", tc.name}, {"arguments", tc.arguments}}},
                });
            }
            assistant["tool_calls"] = calls;
        }
        record(assistant);

        if (turn.tool_calls.empty()) {
            return turn.content;  // final answer
        }

        // Execute each requested tool and feed results back to the model.
        for (const auto& tc : turn.tool_calls) {
            std::string result;
            const Tool* tool = tools_.find(tc.name);
            if (!tool) {
                result = "error: unknown tool '" + tc.name + "'";
            } else {
                Json args;
                try {
                    args = tc.arguments.empty() ? Json::object() : Json::parse(tc.arguments);
                } catch (const std::exception&) {
                    args = Json::object();
                }
                try {
                    result = tool->invoke(args);
                } catch (const std::exception& e) {
                    result = std::string("error: ") + e.what();
                }
            }
            if (on_tool) on_tool(tc.name, result);
            record({
                {"role", "tool"},
                {"tool_call_id", tc.id},
                {"name", tc.name},
                {"content", result},
            });
        }
    }
    return "(stopped: tool-call limit reached)";
}

}  // namespace pi
