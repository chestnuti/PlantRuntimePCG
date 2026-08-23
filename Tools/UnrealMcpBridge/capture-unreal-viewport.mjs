import { spawn } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import readline from "node:readline";
import { fileURLToPath } from "node:url";

const outputPath = process.argv[2];
if (!outputPath) {
  throw new Error("Usage: node capture-unreal-viewport.mjs <output.png>");
}

const bridgePath = new URL("./unreal-mcp-bridge.mjs", import.meta.url);
const bridge = spawn(process.execPath, [fileURLToPath(bridgePath)], {
  stdio: ["pipe", "pipe", "inherit"],
  windowsHide: true,
});
const lines = readline.createInterface({ input: bridge.stdout, crlfDelay: Infinity });
const pending = new Map();
let nextId = 1;

lines.on("line", (line) => {
  const message = JSON.parse(line);
  const handler = pending.get(message.id);
  if (handler) {
    pending.delete(message.id);
    handler(message);
  }
});

function request(method, params) {
  return new Promise((resolve, reject) => {
    const id = nextId++;
    pending.set(id, (message) => message.error ? reject(new Error(message.error.message)) : resolve(message.result));
    bridge.stdin.write(`${JSON.stringify({ jsonrpc: "2.0", id, method, params })}\n`);
  });
}

function callTool(toolsetName, toolName, args) {
  return request("tools/call", {
    name: "call_tool",
    arguments: { toolset_name: toolsetName, tool_name: toolName, arguments: args },
  });
}

await request("initialize", {
  protocolVersion: "2025-06-18",
  capabilities: {},
  clientInfo: { name: "capture-unreal-viewport", version: "1.0" },
});

const cameraResult = await callTool("EditorToolset.EditorAppToolset", "GetCameraTransform", {});
const cameraPayload = JSON.parse(cameraResult.content[0].text);
const cameraTransform = cameraPayload.returnValue;
const captureResult = await callTool("EditorToolset.EditorAppToolset", "CaptureViewport", {
  captureTransform: cameraTransform,
  annotations: [],
  includeEditorOverlays: false,
});
const capturePayload = JSON.parse(captureResult.content[0].text);
const image = capturePayload.returnValue.image;
if (image.mimeType !== "image/png" || !image.data) {
  throw new Error("Unreal MCP did not return a PNG image.");
}

fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, Buffer.from(image.data, "base64"));
bridge.stdin.end();
console.log(outputPath);
