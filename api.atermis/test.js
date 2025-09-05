
const express = require("express");
const http = require("http");
const WebSocket = require("ws");
const cors = require("cors");
const db = require("./db/index.js");

const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());

app.get("/api.artemis", (req, res) => {
  res.send("Hello World!");
});

app.get("/api.artemis/get", async (req, res) => {
  const result = await db.query(
    "SELECT * FROM public.qr_control ORDER BY id DESC"
  );
  res.json(result.rows);
});

// Tạo HTTP server
const server = http.createServer(app);

// Tạo WebSocket server
const wss = new WebSocket.Server({ server, path: "/api.artemis/socket" });

wss.on("connection", (ws) => {
  console.log("✅ Client connected");

  ws.on("message", async (msg) => {
    try {
      const data = JSON.parse(msg); // { qr_code, user_id }

      // Lưu vào DB
      await db.query(
        `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP)`,
        [data.qr_code, data.user_id]
      );

      // Lấy date mới nhất
      const scanResult = await db.query(
        `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
        [data.qr_code]
      );

      const { date } = scanResult.rows[0];

      // Broadcast cho tất cả client
      const payload = JSON.stringify({ qr_code: data.qr_code, user_id: data.user_id, date });
      wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) client.send(payload);
      });

    } catch (err) {
      console.error("Error processing message:", err);
    }
  });

  ws.on("close", () => console.log("❌ Client disconnected"));
});

server.listen(PORT, () => console.log(`Server running at http://localhost:${PORT}`));
