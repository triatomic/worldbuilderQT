"""Call WorldBuilder MCP tools from the command line.

Usage:
  python tools/mcp-call.py <tool> [key=value ...]
  python tools/mcp-call.py --list

Values are parsed as JSON when possible, else treated as a string:
  python tools/mcp-call.py add_object template=AmericaTankCrusader x=500 y=500
"""
from __future__ import annotations

import json
import subprocess
import sys


def rpc(messages: list[dict]) -> list[dict]:
    payload = "".join(json.dumps(m) + "\n" for m in messages)
    proc = subprocess.run(
        [sys.executable, "-m", "tools.worldbuilder_mcp"],
        input=payload, capture_output=True, text=True, timeout=180,
    )
    out = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("{"):
            out.append(json.loads(line))
    if not out and proc.stderr:
        print(proc.stderr.strip(), file=sys.stderr)
    return out


def coerce(text: str):
    try:
        return json.loads(text)
    except ValueError:
        return text


def main(argv: list[str]) -> int:
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    init = {"jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"protocolVersion": "2024-11-05", "capabilities": {},
                       "clientInfo": {"name": "mcp-call", "version": "1"}}}

    if argv[0] == "--list":
        for m in rpc([init, {"jsonrpc": "2.0", "id": 2, "method": "tools/list"}]):
            if m.get("id") == 2:
                for t in m["result"]["tools"]:
                    print(f"{t['name']:<24} {t.get('description','')[:90]}")
        return 0

    tool, args = argv[0], {}
    for pair in argv[1:]:
        if "=" not in pair:
            print(f"expected key=value, got {pair!r}", file=sys.stderr)
            return 2
        key, value = pair.split("=", 1)
        args[key] = coerce(value)

    call = {"jsonrpc": "2.0", "id": 2, "method": "tools/call",
            "params": {"name": tool, "arguments": args}}
    for m in rpc([init, call]):
        if m.get("id") != 2:
            continue
        result = m.get("result", {})
        text = result.get("content", [{}])[0].get("text", "")
        try:
            print(json.dumps(json.loads(text), indent=2))
        except ValueError:
            print(text)
        return 1 if result.get("isError") else 0
    print("no response from server", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
