#include "pi/builtin_tools.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "pi/c/subprocess.h"

namespace fs = std::filesystem;

namespace pi {

namespace {

Tool makeRunShell() {
    Tool tool;
    tool.name = "run_shell";
    tool.description = "Run a shell command and return its combined stdout/stderr.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {{"command", {{"type", "string"}, {"description", "The shell command to run"}}}}},
        {"required", Json::array({"command"})},
    };
    tool.invoke = [](const Json& args) -> std::string {
        std::string command = args.value("command", "");
        if (command.empty()) return "error: missing 'command'";
        pi_subprocess_result r = pi_subprocess_run(command.c_str());
        std::string out(r.output ? r.output : "", r.output_len);
        pi_subprocess_free(&r);
        return "exit_code=" + std::to_string(r.exit_code) + "\n" + out;
    };
    return tool;
}

Tool makeReadFile() {
    Tool tool;
    tool.name = "read_file";
    tool.description = "Read a file and return its contents.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {{"path", {{"type", "string"}, {"description", "Path of the file to read"}}}}},
        {"required", Json::array({"path"})},
    };
    tool.invoke = [](const Json& args) -> std::string {
        const std::string path = args.value("path", std::string{});
        if (path.empty()) return "error: missing 'path'";
        std::ifstream in(path, std::ios::binary);
        if (!in) return "error: failed to open '" + path + "'";
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    return tool;
}

Tool makeWriteFile() {
    Tool tool;
    tool.name = "write_file";
    tool.description = "Create or overwrite a file with the given content.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Path of the file to write"}}},
            {"content", {{"type", "string"}, {"description", "Bytes to write"}}},
        }},
        {"required", Json::array({"path", "content"})},
    };
    tool.invoke = [](const Json& args) -> std::string {
        const std::string path = args.value("path", std::string{});
        const std::string content = args.value("content", std::string{});
        if (path.empty()) return "error: missing 'path'";
        try {
            const fs::path p(path);
            if (p.has_parent_path() && !p.parent_path().empty()) {
                fs::create_directories(p.parent_path());
            }
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) return "error: failed to open '" + path + "'";
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!out) return "error: failed to write '" + path + "'";
            return "ok: wrote " + std::to_string(content.size()) + " bytes to " + path;
        } catch (const fs::filesystem_error& e) {
            return std::string("error: ") + e.what();
        }
    };
    return tool;
}

Tool makeEditFile() {
    Tool tool;
    tool.name = "edit_file";
    tool.description = "Replace exactly one occurrence of a substring in a file.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Path of the file to edit"}}},
            {"old", {{"type", "string"}, {"description", "Exact substring that must occur once"}}},
            {"new", {{"type", "string"}, {"description", "Replacement text"}}},
        }},
        {"required", Json::array({"path", "old", "new"})},
    };
    tool.invoke = [](const Json& args) -> std::string {
        const std::string path = args.value("path", std::string{});
        const std::string old = args.value("old", std::string{});
        const std::string neu = args.value("new", std::string{});
        if (path.empty()) return "error: missing 'path'";
        std::ifstream in(path, std::ios::binary);
        if (!in) return "error: failed to open '" + path + "'";
        std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        // find("") matches at every offset; treat empty old as zero matches.
        std::size_t found = 0;
        std::size_t pos = 0;
        if (!old.empty()) {
            while ((pos = text.find(old, pos)) != std::string::npos) {
                ++found;
                pos += old.size();
            }
        }
        if (found != 1) {
            return "error: expected exactly one match, found " + std::to_string(found);
        }
        text.replace(text.find(old), old.size(), neu);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return "error: failed to open '" + path + "'";
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) return "error: failed to write '" + path + "'";
        return "ok: edited " + path;
    };
    return tool;
}

Tool makeListDir() {
    Tool tool;
    tool.name = "list_dir";
    tool.description = "List directory entries, one per line. Directories are suffixed with /.";
    tool.parameters = {
        {"type", "object"},
        {"properties", {{"path", {{"type", "string"}, {"description", "Directory to list (defaults to .)"}}}}},
    };
    tool.invoke = [](const Json& args) -> std::string {
        std::string path = args.value("path", std::string{});
        if (path.empty()) path = ".";
        try {
            if (!fs::is_directory(path)) return "error: not a directory: " + path;
            std::string out;
            for (const auto& entry : fs::directory_iterator(path)) {
                out += entry.path().filename().string();
                if (entry.is_directory()) out += '/';
                out += '\n';
            }
            return out;
        } catch (const fs::filesystem_error& e) {
            return std::string("error: ") + e.what();
        }
    };
    return tool;
}

}  // namespace

std::vector<Tool> makeBuiltinTools() {
    return {
        makeRunShell(),
        makeReadFile(),
        makeWriteFile(),
        makeEditFile(),
        makeListDir(),
    };
}

}  // namespace pi
