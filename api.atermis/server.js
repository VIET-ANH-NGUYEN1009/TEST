const express = require("express");
const { createServer } = require("node:http");
const { Server } = require("socket.io");
var cors = require("cors");
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
const io = new Server(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"],
  },
  path: "/api.artemis/socket",
  //transports: ["websocket"],
});

io.on("connection", (socket) => {
  socket.on("scan", async ({ qr_code, user_id }) => {
    await db.query(
      `INSERT INTO public.qr_control (qr_code, user_id, date) values ($1, $2, CURRENT_TIMESTAMP)`,
      [qr_code, user_id]
    );

    const scanResult = await db.query(
      `SELECT date from public.qr_control where qr_code = $1 ORDER BY id DESC LIMIT 1`,
      [qr_code]
    );

    const { date } = { ...scanResult.rows[0] };

    io.emit("scan", { qr_code, user_id, date });
  });
});

server.listen(PORT, () => {
  console.log(`Server is running at: http://localhost:${PORT}`);
});
