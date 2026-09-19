// C++ body: the coding-agent CLI entrypoint. Wires the C boundary layer
// (terminal, subprocess) and the C++ core (LLM client, agent, session store,
// plugins) into an interactive/one-shot agent.

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "cairn/agent.hpp"
#include "cairn/builtin_tools.hpp"
#include "cairn/c/terminal.h"
#include "cairn/llm_client.hpp"
#include "cairn/plugin_loader.hpp"
#include "cairn/session_store.hpp"
#include "cairn/tool.hpp"

namespace {

const char* kReset = "\033[0m";
const char* kDim = "\033[2m";
const char* kCyan = "\033[36m";
const char* kGreen = "\033[32m";
const char* kYellow = "\033[33m";

std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
}

}  // namespace

int main(int argc, char** argv) {
    std::string base_url = envOr("CAIRN_BASE_URL", "http://127.0.0.1:8799");
    std::string api_key = envOr("CAIRN_API_KEY", "");
    std::string model = envOr("CAIRN_MODEL", "gpt-4o-mini");
    std::string db_path = envOr("CAIRN_DB", "cairn-session.db");
    std::string session = "default";
    std::string one_shot;
    std::string plugin_path;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--prompt") one_shot = next();
        else if (a == "--session") session = next();
        else if (a == "--plugin") plugin_path = next();
        else if (a == "--model") model = next();
        else if (a == "--base-url") base_url = next();
    }

    cairn::ToolRegistry tools;
    for (auto& tool : cairn::makeBuiltinTools()) {
        tools.add(std::move(tool));
    }
    if (!plugin_path.empty()) {
        try {
            int n = cairn::loadPlugin(plugin_path, tools);
            std::cerr << kDim << "[loaded plugin: " << plugin_path << " (+" << n
                      << " tool(s))]" << kReset << "\n";
        } catch (const std::exception& e) {
            std::cerr << "plugin load error: " << e.what() << "\n";
        }
    }

    cairn::LlmClient client(base_url, api_key, model);
    cairn::SessionStore store(db_path);
    cairn::Agent agent(client, tools, store,
                    session, "You are Cairn, a concise coding agent.");

    cairn_term_size sz = cairn_term_get_size();
    std::cerr << kCyan << "Cairn (native C/C++)  " << kReset << kDim << model
              << "  @ " << base_url << "  [term " << sz.rows << "x" << sz.cols
              << "]" << kReset << "\n";

    auto on_token = [](const std::string& piece) {
        std::cout << piece << std::flush;
    };
    auto on_tool = [](const std::string& name, const std::string& result) {
        std::cout << "\n" << kYellow << "  \xE2\x8A\xB3 tool " << name << kReset << kDim
                  << " -> " << result << kReset << "\n";
    };

    auto turn = [&](const std::string& input) {
        std::cout << kGreen << "\n> " << kReset << input << "\n";
        std::string final = agent.run(input, on_token, on_tool);
        (void)final;
        std::cout << "\n";
    };

    if (!one_shot.empty()) {
        turn(one_shot);
        return 0;
    }

    // Interactive REPL (line mode).
    std::cout << kDim << "Type a message, or Ctrl-D to exit." << kReset << "\n";
    std::string line;
    while (true) {
        std::cout << kGreen << "> " << kReset << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        std::string final = agent.run(line, on_token, on_tool);
        (void)final;
        std::cout << "\n";
    }
    return 0;
}
