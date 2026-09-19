#include "cairn/plugin_loader.hpp"

#include <dlfcn.h>

#include <stdexcept>

#include "cairn/c/plugin.h"

namespace cairn {

int loadPlugin(const std::string& so_path, ToolRegistry& registry) {
    // Keep the handle open for the process lifetime; tool fns live in the .so.
    void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
    }

    dlerror();
    auto reg = reinterpret_cast<cairn_plugin_register_fn>(dlsym(handle, "cairn_plugin_register"));
    const char* sym_err = dlerror();
    if (sym_err || !reg) {
        throw std::runtime_error(std::string("missing cairn_plugin_register: ") +
                                 (sym_err ? sym_err : "null symbol"));
    }

    const cairn_plugin_manifest* manifest = reg();
    if (!manifest) {
        throw std::runtime_error("plugin returned null manifest");
    }
    if (manifest->abi_version != CAIRN_PLUGIN_ABI_VERSION) {
        throw std::runtime_error("plugin ABI mismatch (got " +
                                 std::to_string(manifest->abi_version) + ", want " +
                                 std::to_string(CAIRN_PLUGIN_ABI_VERSION) + ")");
    }

    int added = 0;
    for (int i = 0; i < manifest->tool_count; ++i) {
        const cairn_plugin_tool& pt = manifest->tools[i];
        Tool tool;
        tool.name = pt.name ? pt.name : "";
        tool.description = pt.description ? pt.description : "";
        tool.parameters = pt.params_json ? Json::parse(pt.params_json) : Json::object();

        cairn_plugin_tool_fn invoke = pt.invoke;
        cairn_plugin_free_fn free_fn = pt.free_result;
        tool.invoke = [invoke, free_fn](const Json& args) -> std::string {
            const std::string args_str = args.dump();
            char* raw = invoke(args_str.c_str());
            if (!raw) return "";
            std::string result(raw);
            if (free_fn) free_fn(raw);
            return result;
        };
        registry.add(std::move(tool));
        ++added;
    }
    return added;
}

}  // namespace cairn
