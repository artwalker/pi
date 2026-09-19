#pragma once
// C++ body: bridges C-ABI plugin shared objects into the C++ ToolRegistry.

#include <string>

#include "cairn/tool.hpp"

namespace cairn {

// dlopen the given .so, read its cairn_plugin_register() manifest, and register
// each exported tool into `registry`. Returns the number of tools added.
// Throws std::runtime_error on load / ABI-version failure.
int loadPlugin(const std::string& so_path, ToolRegistry& registry);

}  // namespace cairn
