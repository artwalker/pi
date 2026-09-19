/*
 * C ABI plugin boundary.
 *
 * Cairn is a "self-extensible" agent. In a compiled world you cannot hot-load
 * source, so the extension boundary is a stable C ABI: a plugin is a shared
 * object exposing cairn_plugin_register(), loaded via dlopen. C (not C++) is the
 * right language for this contract because the C ABI is stable across
 * compilers and toolchain versions, whereas the C++ ABI is not.
 *
 * A plugin provides one or more tools the agent can call. Each tool receives a
 * JSON-encoded arguments string and returns a newly-allocated JSON/text string
 * that the host frees with the provided free function.
 */
#ifndef CAIRN_C_PLUGIN_H
#define CAIRN_C_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define CAIRN_PLUGIN_ABI_VERSION 1

/* Returns a heap string (host frees it via cairn_plugin_tool.free_result). */
typedef char *(*cairn_plugin_tool_fn)(const char *args_json);
typedef void (*cairn_plugin_free_fn)(char *result);

typedef struct {
    const char       *name;         /* tool name exposed to the model */
    const char       *description;  /* human/model-facing description */
    const char       *params_json;  /* JSON Schema for the arguments object */
    cairn_plugin_tool_fn invoke;
    cairn_plugin_free_fn free_result;
} cairn_plugin_tool;

typedef struct {
    int                   abi_version;   /* must equal CAIRN_PLUGIN_ABI_VERSION */
    const char           *plugin_name;
    const cairn_plugin_tool *tools;
    int                   tool_count;
} cairn_plugin_manifest;

/* Every plugin .so must export this symbol. */
typedef const cairn_plugin_manifest *(*cairn_plugin_register_fn)(void);
const cairn_plugin_manifest *cairn_plugin_register(void);

#ifdef __cplusplus
}
#endif

#endif /* CAIRN_C_PLUGIN_H */
