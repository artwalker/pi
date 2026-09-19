#pragma once
// C++ body: streaming LLM client.
//
// Wraps libcurl (a C library) with RAII and C++ callbacks. Speaks the OpenAI
// /v1/chat/completions wire protocol with stream=true and parses the SSE
// event stream incrementally, surfacing content deltas as they arrive.

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "cairn/tool.hpp"

namespace cairn {

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // raw JSON string
};

struct AssistantTurn {
    std::string           content;     // final assistant text (may be empty)
    std::vector<ToolCall> tool_calls;  // requested tool calls (may be empty)
};

class LlmClient {
public:
    LlmClient(std::string base_url, std::string api_key, std::string model);

    // Stream one assistant turn. `messages` is the OpenAI-format history.
    // `on_token` fires for each streamed content delta.
    AssistantTurn chat(const Json& messages,
                       const ToolRegistry& tools,
                       const std::function<void(const std::string&)>& on_token);

private:
    std::string base_url_;
    std::string api_key_;
    std::string model_;
};

}  // namespace cairn
