// server.js
const express = require("express");
const http = require("http");
const socketIo = require("socket.io"); // Socket.IO v2.x
const cors = require("cors");
const db = require("./db/index.js"); // module db PostgreSQL

const PORT = process.env.PORT || 3000;

const app = express();
app.use(cors());
app.use(express.json());

// --- HTTP routes ---
app.get("/api.artemis", (req, res) => {
  res.send("Hello World!");
});

app.get("/api.artemis/get", async (req, res) => {
  try {
    const result = await db.query(
      "SELECT * FROM public.qr_control ORDER BY id DESC"
    );
    res.json(result.rows);
  } catch (err) {
    console.error("DB error:", err);
    res.status(500).json({ error: "Database query failed" });
  }
});

// --- Create HTTP server ---
const server = http.createServer(app);

// --- Socket.IO v2.x ---
const io = socketIo(server, {
  path: "/api.artemis/socket",
  cors: {
    origin: "*",
    methods: ["GET", "POST"],
  },
});

// --- Handle Socket.IO connections ---
io.on("connection", (socket) => {
  console.log("✅ Client connected:", socket.id);

  socket.on("scan", async ({ qr_code, user_id }) => {
    try {
      // Lưu vào database
      await db.query(
        `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP)`,
        [qr_code, user_id]
      );

      // Lấy bản ghi mới nhất
      const scanResult = await db.query(
        `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
        [qr_code]
      );

      const { date } = scanResult.rows[0];

      // Phát sự kiện tới tất cả client
      io.emit("scan", { qr_code, user_id, date });
      console.log(`📤 Scan emitted: ${qr_code} | ${user_id}`);
    } catch (err) {
      console.error("Error handling scan:", err);
    }
  });

  socket.on("disconnect", () => {
    console.log("❌ Client disconnected:", socket.id);
  });
});

// --- Start server ---
server.listen(PORT, () => {
  console.log(`Server is running at http://localhost:${PORT}`);
});
