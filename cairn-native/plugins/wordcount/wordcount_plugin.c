/*
 * Example Cairn plugin, compiled as a standalone shared object and loaded at
 * runtime via dlopen. It exposes a `word_count` tool. This is the concrete
 * answer to "how is a compiled agent self-extensible": a stable C ABI.
 *
 * Deliberately dependency-free C: it does its own tiny JSON string extraction
 * so a plugin author needs nothing but a C compiler and cairn/c/plugin.h.
 */
#include "cairn/c/plugin.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Extract the string value of "text" from a small JSON object. Minimal, but
 * enough for the demo; a real plugin would link a JSON lib. */
static char *extract_text(const char *json) {
    const char *key = strstr(json, "\"text\"");
    if (!key) return NULL;
    const char *colon = strchr(key, ':');
    if (!colon) return NULL;
    const char *q = strchr(colon, '"');
    if (!q) return NULL;
    q++;
    const char *end = q;
    while (*end && *end != '"') {
        if (*end == '\\' && end[1]) end++;
        end++;
    }
    size_t len = (size_t)(end - q);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, q, len);
    out[len] = '\0';
    return out;
}

static char *word_count_invoke(const char *args_json) {
    char *text = extract_text(args_json ? args_json : "");
    long words = 0;
    long chars = 0;
    if (text) {
        chars = (long)strlen(text);
        int in_word = 0;
        for (const char *p = text; *p; ++p) {
            if (isspace((unsigned char)*p)) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
        free(text);
    }
    char *result = (char *)malloc(128);
    if (!result) return NULL;
    snprintf(result, 128, "{\"words\": %ld, \"chars\": %ld}", words, chars);
    return result;
}

static void free_result(char *result) { free(result); }

static const cairn_plugin_tool k_tools[] = {
    {
        "word_count",
        "Count the words and characters in a piece of text.",
        "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
        word_count_invoke,
        free_result,
    },
};

static const cairn_plugin_manifest k_manifest = {
    CAIRN_PLUGIN_ABI_VERSION,
    "wordcount",
    k_tools,
    1,
};

const cairn_plugin_manifest *cairn_plugin_register(void) { return &k_manifest; }
