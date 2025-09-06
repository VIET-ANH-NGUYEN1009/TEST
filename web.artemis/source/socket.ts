import { io } from "socket.io-client";

// "undefined" means the URL will be computed from the `window.location` object
const URL = "http://10.0.108.10:99";

export const socket = io(URL, {
  path: "/api.artemis/socket",
});
