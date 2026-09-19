#!/usr/bin/env python3
"""Deterministic end-to-end check for the native filesystem tools.

Stands up an OpenAI-compatible SSE server that drives the agent through a
write -> read -> edit -> list sequence, runs the real `pi` binary against it,
then asserts the on-disk result. Exits nonzero on any failure.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = 8801
WORKDIR = tempfile.mkdtemp(prefix="pi_fs_verify_")
TARGET = os.path.join(WORKDIR, "notes", "hello.txt")


def sse(obj):
    return f"data: {json.dumps(obj)}\n\n".encode()


def chunk(delta, finish=None):
    return {"choices": [{"index": 0, "delta": delta, "finish_reason": finish}]}


def tool_call(name, args):
    return {"tool_calls": [{"index": 0, "id": "c", "type": "function",
                            "function": {"name": name, "arguments": json.dumps(args)}}]}


SCRIPT = [
    ("write_file", {"path": TARGET, "content": "hello\nworld\n"}),
    ("read_file", {"path": TARGET}),
    ("edit_file", {"path": TARGET, "old": "world", "new": "pi"}),
    ("list_dir", {"path": os.path.join(WORKDIR, "notes")}),
]


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(n) or b"{}")
        done = sum(1 for m in body.get("messages", []) if m.get("role") == "tool")
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()

        def emit(o):
            self.wfile.write(sse(o))
            self.wfile.flush()

        if done < len(SCRIPT):
            name, args = SCRIPT[done]
            emit(chunk({"role": "assistant"}))
            emit(chunk(tool_call(name, args)))
            emit(chunk({}, finish="tool_calls"))
        else:
            for w in ["All", "four", "filesystem", "tools", "ran."]:
                emit(chunk({"content": w + " "}))
            emit(chunk({}, finish="stop"))
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


def main():
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    time.sleep(0.3)

    binary = os.path.join(os.path.dirname(__file__), "..", "build", "pi")
    env = dict(os.environ, PI_DB=os.path.join(WORKDIR, "s.db"))
    proc = subprocess.run(
        [binary, "--base-url", f"http://127.0.0.1:{PORT}", "--session", "fs",
         "--prompt", "create notes/hello.txt, read it, fix a typo, list the dir"],
        env=env, capture_output=True, text=True, timeout=60)
    srv.shutdown()

    print(proc.stdout)

    failures = []
    if not os.path.exists(TARGET):
        failures.append(f"file not created: {TARGET}")
    else:
        got = open(TARGET).read()
        if got != "hello\npi\n":
            failures.append(f"edited content mismatch: {got!r} != 'hello\\npi\\n'")
    if "ok: wrote" not in proc.stdout:
        failures.append("write_file result not observed in output")
    if "ok: edited" not in proc.stdout:
        failures.append("edit_file result not observed in output")
    if "hello.txt" not in proc.stdout:
        failures.append("list_dir did not surface hello.txt")

    shutil.rmtree(WORKDIR, ignore_errors=True)
    if failures:
        print("RESULT: FAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("RESULT: PASS  (write -> read -> edit -> list verified on disk)")


if __name__ == "__main__":
    main()
