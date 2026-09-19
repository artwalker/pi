#pragma once
// C++ body: bridges C-ABI plugin shared objects into the C++ ToolRegistry.

#include <string>

#include "pi/tool.hpp"

namespace pi {

// dlopen the given .so, read its pi_plugin_register() manifest, and register
// each exported tool into `registry`. Returns the number of tools added.
// Throws std::runtime_error on load / ABI-version failure.
int loadPlugin(const std::string& so_path, ToolRegistry& registry);

}  // namespace pi
