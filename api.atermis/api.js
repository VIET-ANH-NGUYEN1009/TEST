require("dotenv").config();
const express = require("express");
const { createServer } = require("http");
const { Server } = require("socket.io");
const WebSocket = require("ws");
const db = require("./db/index.js"); // module bạn đã viết

const PORT = process.env.PORT || 3000;

const app = express();
app.use(express.json());

// Route test HTTP
app.get("/api.artemis", (req, res) => {
  res.send("Hello World!");
});

// Lấy danh sách QR scan
app.get("/api.artemis/get", async (req, res) => {
  try {
    const result = await db.query(
      "SELECT * FROM public.qr_control ORDER BY id DESC"
    );
    res.json(result.rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ message: "Database error" });
  }
});

// Tạo server HTTP
const server = createServer(app);

// Socket.IO v4 cho web client
const io = new Server(server, {
  cors: { origin: "*", methods: ["GET", "POST"] },
  path: "/api.artemis/socket",
});

io.on("connection", (socket) => {
  console.log("Web client connected:", socket.id);
});

// WebSocket riêng cho ESP32
const wss = new WebSocket.Server({ noServer: true });

server.on("upgrade", (req, socket, head) => {
  if (req.url === "/esp") {
    wss.handleUpgrade(req, socket, head, (ws) => {
      wss.emit("connection", ws, req);
    });
  } else {
    socket.destroy(); // path khác thì từ chối
  }
});

wss.on("connection", (ws) => {
  console.log("ESP32 connected via WebSocket");

  ws.on("message", async (message) => {
    try {
      const data = JSON.parse(message); // nhận JSON từ ESP32
      const { qr_code, user_id } = data;

      if (!qr_code || !user_id) {
        return ws.send(JSON.stringify({ error: "Invalid data" }));
      }

      // Insert và trả ngay date
      const result = await db.query(
        `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP) RETURNING date`,
        [qr_code, user_id]
      );

      const { date } = result.rows[0];

      // Phát dữ liệu đến tất cả web client Socket.IO
      io.emit("scan", { qr_code, user_id, date });

      // Gửi phản hồi cho ESP32
      ws.send(JSON.stringify({ success: true, qr_code, date }));
    } catch (err) {
      console.error("Error handling ESP32 message:", err);
      ws.send(JSON.stringify({ error: "Server error" }));
    }
  });
});

// Start server
server.listen(PORT, () =>
  console.log(`Server running at http://localhost:${PORT}`)
);

