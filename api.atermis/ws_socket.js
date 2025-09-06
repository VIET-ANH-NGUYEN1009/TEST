const express = require("express");
const { createServer } = require("http");
const { Server } = require("socket.io");
const WebSocket = require("ws");
const cors = require("cors");
const db = require("./db/index.js");

const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());

// Test API
app.get("/api.artemis", (req, res) => {
  res.send("Hello World!");
});

// Lấy list QR từ DB
app.get("/api.artemis/get", async (req, res) => {
  const result = await db.query(
    "SELECT * FROM public.qr_control ORDER BY id DESC"
  );
  res.json(result.rows);
});

// Tạo HTTP server
const server = createServer(app);

// Socket.IO server (cho React frontend)
const io = new Server(server, {
  cors: { origin: "*", methods: ["GET", "POST"] },
  path: "/api.artemis/socket",
});

// WebSocket server thuần (cho ESP32)
const wss = new WebSocket.Server({ server, path: "/esp" });

wss.on("connection", (ws) => {
  console.log("ESP32 connected via WS");

  ws.on("message", async (message) => {
    try {
      const data = JSON.parse(message.toString());
      const { qr_code, user_id } = data;

      // Lưu DB
      await db.query(
        `INSERT INTO public.qr_control (qr_code, user_id, date) values ($1, $2, CURRENT_TIMESTAMP)`,
        [qr_code, user_id]
      );

      const scanResult = await db.query(
        `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
        [qr_code]
      );

      const { date } = scanResult.rows[0];

      // Emit cho React frontend qua Socket.IO
      io.emit("scan", { qr_code, user_id, date });

      // Trả lời lại ESP32
      ws.send(JSON.stringify({ status: "ok", qr_code, user_id, date }));
    } catch (err) {
      console.error(err);
      ws.send(JSON.stringify({ status: "error", message: err.message }));
    }
  });
});

// Lắng nghe
server.listen(PORT, () => {
  console.log(`✅ Server is running at: http://localhost:${PORT}`);
});
