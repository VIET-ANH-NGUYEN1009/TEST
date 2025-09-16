const express = require("express");
const { createServer } = require("http");
const cors = require("cors");
const { Server } = require("socket.io");
const { WebSocketServer } = require("ws");
const db = require("./db/index.js");

const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());
app.use(express.json());

// --- REST API ---
app.get("/api.artemis", (req, res) => res.send("Hello World!"));

app.get("/api.artemis/get", async (req, res) => {
  const result = await db.query(
    "SELECT * FROM public.qr_control ORDER BY id DESC"
  );
  res.json(result.rows);
});

// --- HTTP server ---
const server = createServer(app);

// --- Socket.IO cho frontend ---
const io = new Server(server, {
  cors: { origin: "*", methods: ["GET", "POST"] },
  path: "/api.artemis/socket",
});

// --- WebSocket chuẩn cho ESP32 ---
const wss = new WebSocketServer({ server, path: "/api.artemis/ws" });

// --- Hàm broadcast dữ liệu cho cả Socket.IO và WS ---
async function broadcastScan(qr_code, user_id) {
  // Lưu vào DB
  await db.query(
    `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP)`,
    [qr_code, user_id]
  );

  // Lấy lại ngày quét gần nhất
  const scanResult = await db.query(
    `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
    [qr_code]
  );

  const { date } = scanResult.rows[0];
  const payload = { qr_code, user_id, date };

  // Gửi cho tất cả client Socket.IO
  io.emit("scan", payload);

  // Gửi cho tất cả client WS
  const payloadStr = JSON.stringify(payload);
  wss.clients.forEach(client => {
    if (client.readyState === client.OPEN) {
      client.send(payloadStr);
    }
  });
}

// --- Socket.IO event ---
io.on("connection", (socket) => {
  console.log("Socket.IO client connected:", socket.id);

  socket.on("scan", ({ qr_code, user_id }) => {
    broadcastScan(qr_code, user_id).catch(console.error);
  });

  socket.on("disconnect", () => console.log("Socket.IO client disconnected:", socket.id));
});

// --- WebSocket event ---
wss.on("connection", (ws) => {
  console.log("WebSocket client connected");

  ws.on("message", (message) => {
    try {
      const data = JSON.parse(message.toString());
      const { qr_code, user_id } = data;
      broadcastScan(qr_code, user_id).catch(console.error);
    } catch (err) {
      console.error(err);
    }
  });

  ws.on("close", () => console.log("WebSocket client disconnected"));
});

// --- Start server ---
server.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}`);
  console.log(`Socket.IO path: /api.artemis/socket`);
  console.log(`WebSocket path: ws://localhost:${PORT}/api.artemis/ws`);
});
