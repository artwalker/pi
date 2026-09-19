#pragma once
// C++ body: tool registry.
//
// A Tool is the unit the model can call. Built-in tools wrap the C boundary
// layer (e.g. subprocess); plugin tools bridge to the C-ABI plugin contract.
// std::function + STL containers make this ergonomic in a way plain C is not,
// which is exactly why the orchestration layer lives in C++.

#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace cairn {

using Json = nlohmann::json;

struct Tool {
    std::string name;
    std::string description;
    Json        parameters;  // JSON Schema object
    std::function<std::string(const Json& args)> invoke;
};

class ToolRegistry {
public:
    void add(Tool tool) { tools_.push_back(std::move(tool)); }

    const Tool* find(const std::string& name) const {
        for (const auto& t : tools_) {
            if (t.name == name) return &t;
        }
        return nullptr;
    }

    const std::vector<Tool>& all() const { return tools_; }

    // Serialize into the OpenAI "tools" array shape.
    Json toOpenAiSchema() const {
        Json arr = Json::array();
        for (const auto& t : tools_) {
            arr.push_back({
                {"type", "function"},
                {"function", {
                    {"name", t.name},
                    {"description", t.description},
                    {"parameters", t.parameters},
                }},
            });
        }
        return arr;
    }

private:
    std::vector<Tool> tools_;
};

}  // namespace cairn
