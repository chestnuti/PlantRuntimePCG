import net from "node:net";
import readline from "node:readline";

const upstreamUrl = new URL(process.env.UNREAL_MCP_URL ?? "http://127.0.0.1:8000/mcp");
const requestTimeoutMs = Number.parseInt(process.env.UNREAL_MCP_TIMEOUT_MS ?? "120000", 10);

let sessionId = "";
let protocolVersion = "2025-06-18";

function writeMessage(message) {
  process.stdout.write(`${JSON.stringify(message)}\n`);
}

function writeError(message) {
  process.stderr.write(`[unreal-mcp-bridge] ${message}\n`);
}

function rpcError(id, code, message) {
  return { jsonrpc: "2.0", id: id ?? null, error: { code, message } };
}

function parseHeaders(headerText) {
  const lines = headerText.split("\r\n");
  const statusMatch = lines.shift()?.match(/^HTTP\/\d(?:\.\d)?\s+(\d+)/i);
  const headers = {};
  for (const line of lines) {
    const separator = line.indexOf(":");
    if (separator > 0) {
      headers[line.slice(0, separator).trim().toLowerCase()] = line.slice(separator + 1).trim();
    }
  }
  return { statusCode: Number.parseInt(statusMatch?.[1] ?? "0", 10), headers };
}

function findSseTerminalMessage(buffer, requestId) {
  for (const line of buffer.replaceAll("\r\n", "\n").split("\n")) {
    const dataIndex = line.indexOf("data:");
    if (dataIndex < 0) {
      continue;
    }

    const data = line.slice(dataIndex + 5).trimStart();
    try {
      const candidate = JSON.parse(data);
      if (candidate?.id === requestId && ("result" in candidate || "error" in candidate)) {
        return candidate;
      }
    } catch {
      // A partial TCP frame is expected; the next data event retries with the accumulated buffer.
    }
  }
  return null;
}

function forwardToUnreal(message) {
  return new Promise((resolve, reject) => {
    const body = JSON.stringify(message);
    const headers = {
      Accept: "application/json, text/event-stream",
      "Content-Type": "application/json",
      "Content-Length": String(Buffer.byteLength(body)),
      "MCP-Protocol-Version": protocolVersion,
      Connection: "keep-alive",
    };

    if (sessionId) {
      headers["Mcp-Session-Id"] = sessionId;
    }

    const headerLines = Object.entries(headers).map(([name, value]) => `${name}: ${value}`);
    const rawRequest = [
      `POST ${upstreamUrl.pathname}${upstreamUrl.search} HTTP/1.1`,
      `Host: ${upstreamUrl.hostname}:${upstreamUrl.port}`,
      ...headerLines,
      "",
      body,
    ].join("\r\n");

    const socket = net.createConnection({ host: upstreamUrl.hostname, port: Number(upstreamUrl.port) });
    let buffer = "";
    let firstResponse = null;
    let firstBodyOffset = -1;
    let settled = false;

    const finish = (value) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeout);
      resolve(value);
      socket.destroy();
    };

    const fail = (error) => {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timeout);
      reject(error);
      socket.destroy();
    };

    const inspectBuffer = () => {
      if (!firstResponse) {
        const headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
          return;
        }
        firstResponse = parseHeaders(buffer.slice(0, headerEnd));
        firstBodyOffset = headerEnd + 4;
        const returnedSessionId = firstResponse.headers["mcp-session-id"];
        if (returnedSessionId) {
          sessionId = returnedSessionId;
        }
      }

      const contentType = String(firstResponse.headers["content-type"] ?? "").toLowerCase();
      const contentLength = Number.parseInt(firstResponse.headers["content-length"] ?? "0", 10);
      const responseBody = buffer.slice(firstBodyOffset);

      if (contentType.includes("text/event-stream")) {
        const terminalMessage = findSseTerminalMessage(responseBody, message.id);
        if (terminalMessage) {
          finish(terminalMessage);
        }
        return;
      }

      if (contentLength === 0) {
        finish(null);
        return;
      }

      if (Buffer.byteLength(responseBody) >= contentLength) {
        const jsonBody = Buffer.from(responseBody).subarray(0, contentLength).toString("utf8");
        try {
          finish(JSON.parse(jsonBody));
        } catch (error) {
          fail(new Error(`UE returned invalid JSON: ${error.message}`));
        }
      }
    };

    const timeout = setTimeout(() => {
      fail(new Error(`UE MCP request timed out after ${requestTimeoutMs} ms`));
    }, requestTimeoutMs);

    socket.setEncoding("utf8");
    socket.on("connect", () => socket.write(rawRequest));
    socket.on("data", (chunk) => {
      buffer += chunk;
      inspectBuffer();
    });
    socket.on("end", () => {
      inspectBuffer();
      if (!settled) {
        fail(new Error("UE closed the connection before returning a complete MCP response"));
      }
    });
    socket.on("error", fail);
  });
}

async function handleMessage(message) {
  if (!message || message.jsonrpc !== "2.0" || typeof message.method !== "string") {
    if (message?.id !== undefined) {
      writeMessage(rpcError(message.id, -32600, "Invalid JSON-RPC request"));
    }
    return;
  }

  if (message.method === "initialize" && typeof message.params?.protocolVersion === "string") {
    protocolVersion = message.params.protocolVersion;
    sessionId = "";
  }

  try {
    const response = await forwardToUnreal(message);
    if (response && message.id !== undefined) {
      writeMessage(response);
    }
  } catch (error) {
    writeError(`${message.method}: ${error.message}`);
    if (message.id !== undefined) {
      writeMessage(rpcError(message.id, -32000, `Unreal MCP bridge error: ${error.message}`));
    }
  }
}

const input = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
let queue = Promise.resolve();

input.on("line", (line) => {
  const trimmed = line.trim();
  if (!trimmed) {
    return;
  }

  let message;
  try {
    message = JSON.parse(trimmed);
  } catch (error) {
    writeError(`Ignoring invalid stdin JSON: ${error.message}`);
    return;
  }

  queue = queue.then(() => handleMessage(message));
});

input.on("close", () => {
  queue.finally(() => process.exit(0));
});
