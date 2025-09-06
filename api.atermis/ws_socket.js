// server.js
const express = require("express");
const { createServer } = require("http");
const { Server } = require("socket.io");
const WebSocket = require("ws");
const cors = require("cors");
const db = require("./db/index.js");

const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());

// --- API test ---
app.get("/api.artemis", (req, res) => res.send("Hello World!"));

// --- Lấy list QR từ DB ---
app.get("/api.artemis/get", async (req, res) => {
  const result = await db.query(
    "SELECT * FROM public.qr_control ORDER BY id DESC"
  );
  res.json(result.rows);
});

// --- HTTP server ---
const server = createServer(app);

// --- Socket.IO cho React frontend ---
const io = new Server(server, {
  cors: { origin: "*", methods: ["GET", "POST"] },
  path: "/api.artemis/socket",
});

io.on("connection", (socket) => {
  console.log("✅ Frontend connected via Socket.IO");

  socket.on("disconnect", () => {
    console.log("❌ Frontend disconnected");
  });
});

// --- WebSocket server cho ESP32 ---
const wss = new WebSocket.Server({ server, path: "/esp" });

wss.on("connection", (ws) => {
  console.log("✅ ESP32 connected via WS");

  ws.on("message", async (msg) => {
    try {
      const data = JSON.parse(msg); // { qr_code, user_id }
      const { qr_code, user_id } = data;

      // Lưu vào DB
      await db.query(
        `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP)`,
        [qr_code, user_id]
      );

      // Lấy date mới nhất
      const scanResult = await db.query(
        `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
        [qr_code]
      );
      const { date } = scanResult.rows[0];

      const payload = { qr_code, user_id, date };
      const payloadStr = JSON.stringify(payload);

      // --- Bridge WS -> Socket.IO ---
      io.emit("scan", payload); // gửi realtime cho frontend React

      // --- Broadcast cho tất cả client WS (ESP32 khác) ---
      wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) client.send(payloadStr);
      });

      // --- Trả lời ESP32 gửi QR ---
      ws.send(JSON.stringify({ status: "ok", ...payload }));

    } catch (err) {
      console.error("Error processing WS message:", err);
      ws.send(JSON.stringify({ status: "error", message: err.message }));
    }
  });

  ws.on("close", () => console.log("❌ ESP32 disconnected"));
});

// --- Start server ---
server.listen(PORT, () =>
  console.log(`✅ Server running at http://localhost:${PORT}`)
);
