# Unreal MCP compatibility bridge

This local bridge adapts Unreal Engine 5.8's experimental HTTP/SSE MCP responses to the newline-delimited stdio transport used by Codex.

## Data flow

`Codex stdio -> bridge -> http://127.0.0.1:8000/mcp -> Unreal Engine`

The bridge forwards MCP session and protocol headers. For tool calls, it reads Unreal's open-ended SSE response until the terminal JSON-RPC result is received, then emits that result to Codex as one stdio message.

## Requirements

- Unreal Editor is running with `ModelContextProtocol` enabled.
- The Unreal MCP server is listening on `127.0.0.1:8000`.
- Node.js 20 or newer is available.

No third-party packages are required.

## Optional environment variables

- `UNREAL_MCP_URL`: upstream URL; defaults to `http://127.0.0.1:8000/mcp`.
- `UNREAL_MCP_TIMEOUT_MS`: request timeout; defaults to `120000`.
