import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const server = new McpServer({
  name: "bitaxe-mcp",
  version: "1.0.0"
});

const BITAXE_IP = "192.168.0.128";

server.tool("get_bitaxe_status",
  "Get current telemetry from Bitaxe (temperature, hashrate, voltage, frequency)",
  {},
  async () => {
    try {
      const response = await fetch(`http://${BITAXE_IP}/api/system/info`);
      if (!response.ok) {
        return { content: [{ type: "text", text: `HTTP Error: ${response.status}` }] };
      }
      const data = await response.json();
      const status = {
        temp: data.temp,
        hashrate: data.hashRate,
        voltage: data.coreVoltage,
        frequency: data.frequency,
        uptimeSeconds: data.uptimeSeconds
      };
      return { content: [{ type: "text", text: JSON.stringify(status, null, 2) }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

server.tool("set_bitaxe_config",
  "Set Bitaxe core frequency and voltage. Allowed Freqs: 400, 490, 525, 550, 600, 625. Allowed Volts: 1000, 1060, 1100, 1150, 1200, 1250.",
  {
    frequency: z.number().int().min(400).max(625),
    coreVoltage: z.number().int().min(1000).max(1250)
  },
  async ({ frequency, coreVoltage }) => {
    try {
      const response = await fetch(`http://${BITAXE_IP}/api/system`, {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ frequency, coreVoltage })
      });
      if (!response.ok) {
        return { content: [{ type: "text", text: `HTTP Error: ${response.status}` }] };
      }
      return { content: [{ type: "text", text: `Successfully updated config to Freq: ${frequency}MHz, Volt: ${coreVoltage}mV` }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

server.tool("emergency_throttle",
  "Instantly drop Bitaxe frequency to 400MHz and voltage to 1000mV to prevent overheating.",
  {},
  async () => {
    try {
      const response = await fetch(`http://${BITAXE_IP}/api/system`, {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ frequency: 400, coreVoltage: 1000 })
      });
      if (!response.ok) {
        return { content: [{ type: "text", text: `HTTP Error: ${response.status}` }] };
      }
      return { content: [{ type: "text", text: `EMERGENCY THROTTLE ACTIVATED. Config set to Freq: 400MHz, Volt: 1000mV` }] };
    } catch (e) {
      return { content: [{ type: "text", text: `Error: ${e.message}` }] };
    }
  }
);

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error("Bitaxe MCP Server running on stdio");
}

main().catch((error) => {
  console.error("Fatal error in main():", error);
  process.exit(1);
});
