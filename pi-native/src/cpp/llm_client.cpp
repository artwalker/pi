#include "pi/llm_client.hpp"

#include <curl/curl.h>

#include <cstring>
#include <map>
#include <stdexcept>

namespace pi {

namespace {

// libcurl write callback: accumulate raw bytes into the SSE line assembler.
struct StreamState {
    std::string buffer;  // unconsumed bytes (partial SSE lines)
    std::string content;
    // Accumulate streamed tool_call deltas keyed by their index.
    std::map<int, ToolCall> tool_calls;
    std::function<void(const std::string&)> on_token;
};

void handleEvent(StreamState& st, const std::string& data) {
    if (data == "[DONE]") return;

    Json ev;
    try {
        ev = Json::parse(data);
    } catch (const std::exception&) {
        return;  // ignore malformed keep-alive chunks
    }

    if (!ev.contains("choices") || ev["choices"].empty()) return;
    const Json& delta = ev["choices"][0].value("delta", Json::object());

    if (delta.contains("content") && delta["content"].is_string()) {
        const std::string piece = delta["content"].get<std::string>();
        if (!piece.empty()) {
            st.content += piece;
            if (st.on_token) st.on_token(piece);
        }
    }

    if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
        for (const auto& tc : delta["tool_calls"]) {
            int idx = tc.value("index", 0);
            ToolCall& call = st.tool_calls[idx];
            if (tc.contains("id") && tc["id"].is_string()) {
                call.id = tc["id"].get<std::string>();
            }
            if (tc.contains("function")) {
                const Json& fn = tc["function"];
                if (fn.contains("name") && fn["name"].is_string()) {
                    call.name += fn["name"].get<std::string>();
                }
                if (fn.contains("arguments") && fn["arguments"].is_string()) {
                    call.arguments += fn["arguments"].get<std::string>();
                }
            }
        }
    }
}

// Parse any complete "data:" SSE lines currently in the buffer.
void drainBuffer(StreamState& st) {
    size_t pos;
    while ((pos = st.buffer.find('\n')) != std::string::npos) {
        std::string line = st.buffer.substr(0, pos);
        st.buffer.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("data:", 0) == 0) {
            std::string data = line.substr(5);
            if (!data.empty() && data.front() == ' ') data.erase(0, 1);
            handleEvent(st, data);
        }
    }
}

size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* st = static_cast<StreamState*>(userdata);
    st->buffer.append(ptr, size * nmemb);
    drainBuffer(*st);
    return size * nmemb;
}

}  // namespace

LlmClient::LlmClient(std::string base_url, std::string api_key, std::string model)
    : base_url_(std::move(base_url)),
      api_key_(std::move(api_key)),
      model_(std::move(model)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

AssistantTurn LlmClient::chat(const Json& messages,
                              const ToolRegistry& tools,
                              const std::function<void(const std::string&)>& on_token) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("failed to init libcurl");

    Json payload = {
        {"model", model_},
        {"messages", messages},
        {"stream", true},
    };
    if (!tools.all().empty()) {
        payload["tools"] = tools.toOpenAiSchema();
    }
    const std::string body = payload.dump();
    const std::string url = base_url_ + "/v1/chat/completions";

    StreamState st;
    st.on_token = on_token;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    std::string auth;
    if (!api_key_.empty()) {
        auth = "Authorization: Bearer " + api_key_;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &st);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("request failed: ") + curl_easy_strerror(rc));
    }
    if (http_code >= 400) {
        throw std::runtime_error("HTTP " + std::to_string(http_code) + ": " + st.content);
    }

    AssistantTurn turn;
    turn.content = st.content;
    for (auto& [idx, call] : st.tool_calls) {
        (void)idx;
        if (!call.name.empty()) turn.tool_calls.push_back(std::move(call));
    }
    return turn;
}

}  // namespace pi
