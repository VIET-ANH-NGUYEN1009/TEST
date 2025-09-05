const express = require("express");
const { createServer } = require("http");  
const socketIo = require("socket.io");     
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

const server = createServer(app);
const io = socketIo(server, {
  path: "/api.artemis/socket",
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

io.on("connection", socket => {
  console.log("✅ Client connected");

  socket.on("scan", async ({ qr_code, user_id }) => {
    await db.query(
      `INSERT INTO public.qr_control (qr_code, user_id, date) VALUES ($1, $2, CURRENT_TIMESTAMP)`,
      [qr_code, user_id]
    );

    const scanResult = await db.query(
      `SELECT date FROM public.qr_control WHERE qr_code = $1 ORDER BY id DESC LIMIT 1`,
      [qr_code]
    );

    const { date } = scanResult.rows[0];
    io.emit("scan", { qr_code, user_id, date });
  });
});

server.listen(PORT, () => {
  console.log(`Server is running at: http://localhost:${PORT}`);
});
