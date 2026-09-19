# pi-native

A native **C++/C** implementation of the Pi agent core. This is a foundation
that mirrors the TypeScript packages (`ai`, `agent`, `durable`, `tui`,
`coding-agent`) while deliberately splitting work between the two languages by
their strengths:

| Concern | Language | Why |
|---|---|---|
| Terminal control (raw mode, size) | **C** (`src/c/terminal.c`) | Thin `termios`/`ioctl` syscall layer; nothing for C++ to abstract. |
| Subprocess execution (tool sandbox) | **C** (`src/c/subprocess.c`) | `fork`/`exec`/`pipe` fd plumbing; manual control matters. |
| Plugin boundary | **C ABI** (`include/pi/c/plugin.h`) | The C ABI is stable across compilers/versions; the C++ ABI is not. Extensions ship as `.so` loaded via `dlopen`. |
| LLM client (HTTP + SSE streaming, tool-calls) | **C++** (`src/cpp/llm_client.cpp`) | RAII over libcurl, incremental SSE parsing, `std::function` callbacks. |
| Agent loop + tool registry | **C++** (`src/cpp/agent.cpp`) | STL containers, `std::function`, exception-based error handling. |
| Durable session store | **C++** over SQLite (`src/cpp/session_store.cpp`) | RAII wrapper around the SQLite C library. |
| CLI wiring | **C++** (`src/cpp/main.cpp`) | Ties the C boundary layer and C++ core together. |

The design principle: **high-level orchestration in C++, syscall/ABI edges in C.**
This is the same principle the upstream TypeScript repo follows when it drops to
C only for `packages/tui/native` platform glue — just applied across the board.

## Dependencies

- CMake >= 3.20, a C11 + C++20 toolchain (tested with GCC 13)
- libcurl, SQLite3, nlohmann/json

```bash
sudo apt-get install -y libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev
```

## Build

```bash
cmake -S . -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build -j
```

Produces `build/pi` (the agent CLI) and `build/wordcount.so` (an example plugin).

## Run

`pi` speaks the OpenAI `/v1/chat/completions` wire protocol with `stream=true`,
so it works against any compatible endpoint.

```bash
# Against a real provider:
export PI_BASE_URL=https://api.openai.com
export PI_API_KEY=sk-...
export PI_MODEL=gpt-4o-mini
./build/pi                                   # interactive REPL
./build/pi --prompt "list files with a shell command"   # one-shot

# Load a runtime plugin (C-ABI shared object):
./build/pi --plugin ./build/wordcount.so --prompt "count the words here"
```

Environment / flags: `PI_BASE_URL` / `--base-url`, `PI_API_KEY`, `PI_MODEL` /
`--model`, `PI_DB` (SQLite path), `--session <id>`, `--plugin <path.so>`,
`--prompt <text>` (one-shot).

## End-to-end demo without credentials

`test/mock_server.py` is a local OpenAI-compatible SSE server that stands in for
a provider so the streaming + tool-call loop can be exercised offline. It is a
test stand-in, **not** part of the agent.

```bash
python3 test/mock_server.py &          # listens on 127.0.0.1:8799
./build/pi --base-url http://127.0.0.1:8799 --prompt "greet me from the shell"
```

## Status

Implemented: streaming client, tool-calling agent loop, C subprocess tool,
C-ABI dlopen plugins, SQLite session persistence/resume, terminal-size aware CLI.

Not yet ported from the TS packages: the differential TUI renderer, the multi
provider model catalog, RPC client/server split, and richer built-in tools.
