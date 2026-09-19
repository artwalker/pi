// C++ body: the coding-agent CLI entrypoint. Wires the C boundary layer
// (terminal, subprocess) and the C++ core (LLM client, agent, session store,
// plugins) into an interactive/one-shot agent.

#include <cstdlib>
#include <iostream>
#include <string>

#include "pi/agent.hpp"
#include "pi/c/subprocess.h"
#include "pi/c/terminal.h"
#include "pi/llm_client.hpp"
#include "pi/plugin_loader.hpp"
#include "pi/session_store.hpp"
#include "pi/tool.hpp"

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

// Built-in tool backed by the C subprocess boundary layer.
pi::Tool makeShellTool() {
    pi::Tool tool;
    tool.name = "run_shell";
    tool.description = "Run a shell command and return its combined stdout/stderr.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {{"command", {{"type", "string"}, {"description", "The shell command to run"}}}}},
        {"required", pi::Json::array({"command"})},
    };
    tool.invoke = [](const pi::Json& args) -> std::string {
        std::string command = args.value("command", "");
        if (command.empty()) return "error: missing 'command'";
        pi_subprocess_result r = pi_subprocess_run(command.c_str());
        std::string out(r.output ? r.output : "", r.output_len);
        pi_subprocess_free(&r);
        return "exit_code=" + std::to_string(r.exit_code) + "\n" + out;
    };
    return tool;
}

}  // namespace

int main(int argc, char** argv) {
    std::string base_url = envOr("PI_BASE_URL", "http://127.0.0.1:8799");
    std::string api_key = envOr("PI_API_KEY", "");
    std::string model = envOr("PI_MODEL", "gpt-4o-mini");
    std::string db_path = envOr("PI_DB", "pi-session.db");
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

    pi::ToolRegistry tools;
    tools.add(makeShellTool());
    if (!plugin_path.empty()) {
        try {
            int n = pi::loadPlugin(plugin_path, tools);
            std::cerr << kDim << "[loaded plugin: " << plugin_path << " (+" << n
                      << " tool(s))]" << kReset << "\n";
        } catch (const std::exception& e) {
            std::cerr << "plugin load error: " << e.what() << "\n";
        }
    }

    pi::LlmClient client(base_url, api_key, model);
    pi::SessionStore store(db_path);
    pi::Agent agent(client, tools, store,
                    session, "You are Pi, a concise coding agent.");

    pi_term_size sz = pi_term_get_size();
    std::cerr << kCyan << "Pi (native C/C++)  " << kReset << kDim << model
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
