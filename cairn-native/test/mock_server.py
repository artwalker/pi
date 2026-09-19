#!/usr/bin/env python3
"""Minimal OpenAI-compatible /v1/chat/completions server with SSE streaming.

This is NOT part of the agent; it stands in for a real LLM provider so the
native client's streaming + tool-call loop can be exercised end-to-end without
credentials. It speaks the real wire protocol (data: {...}\n\n ... [DONE]).

Behavior is scripted to drive the agent loop:
  - If the history contains no tool result yet, respond with a tool_call.
  - Once a tool result is present, stream a final natural-language answer.
"""
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST, PORT = "127.0.0.1", 8799


def sse(obj):
    return f"data: {json.dumps(obj)}\n\n".encode()


def chunk(delta, finish=None):
    return {
        "id": "chatcmpl-mock",
        "object": "chat.completion.chunk",
        "model": "mock-gpt",
        "choices": [{"index": 0, "delta": delta, "finish_reason": finish}],
    }


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):  # keep the demo output clean
        pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length) or b"{}")
        messages = body.get("messages", [])
        has_tool_pick = None
        for m in messages:
            if m.get("role") == "tool" and m.get("name") == "word_count":
                has_tool_pick = "wordcount"
            elif m.get("role") == "tool" and m.get("name") == "run_shell":
                has_tool_pick = has_tool_pick or "shell"

        # Latest user text (to decide which tool to exercise).
        user_text = ""
        for m in messages:
            if m.get("role") == "user":
                user_text = m.get("content", "")

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()

        def emit(obj):
            self.wfile.write(sse(obj))
            self.wfile.flush()
            time.sleep(0.03)

        if has_tool_pick is None:
            # First round: ask for a tool call. Pick based on the prompt.
            if "count" in user_text.lower() or "word" in user_text.lower():
                args = json.dumps({"text": user_text})
                name = "word_count"
            else:
                args = json.dumps({"command": "echo hello from the C subprocess layer && uname -s"})
                name = "run_shell"
            emit(chunk({"role": "assistant"}))
            emit(chunk({"tool_calls": [{
                "index": 0,
                "id": "call_1",
                "type": "function",
                "function": {"name": name, "arguments": ""},
            }]}))
            # Stream the arguments in two pieces to exercise delta accumulation.
            mid = len(args) // 2
            emit(chunk({"tool_calls": [{"index": 0, "function": {"arguments": args[:mid]}}]}))
            emit(chunk({"tool_calls": [{"index": 0, "function": {"arguments": args[mid:]}}]}))
            emit(chunk({}, finish="tool_calls"))
        else:
            # Second round: stream a final answer token by token.
            answer = "Done. The tool ran natively and returned above; " \
                     "the C layer handled the syscalls and C++ drove the loop."
            for word in answer.split(" "):
                emit(chunk({"content": word + " "}))
            emit(chunk({}, finish="stop"))

        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


if __name__ == "__main__":
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
