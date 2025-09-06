import { useEffect, useState } from "react";
import "./App.css";
import { socket } from "./socket";
import { ConnectionState } from "./components/ConnectionState";
import { ConnectionManager } from "./components/ConnectionManager";
import { MyForm } from "./components/MyForm";
import List from "./components/List";
import { useQRCode } from "./lib/hooks";
import type { QRList } from "./lib/types";
import { Separator } from "./components/ui/separator";

function App() {
  const [isConnected, setIsConnected] = useState(socket.connected);
  const { list, setList } = useQRCode();

  useEffect(() => {
    const onConnect = () => {
      setIsConnected(true);
    };

    const onDisconnect = () => {
      setIsConnected(false);
    };

    const onScanEvent = (QRList: QRList) => {
      setList((prev) => [QRList, ...prev]);
    };

    socket.on("connect", onConnect);
    socket.on("disconnect", onDisconnect);
    socket.on("scan", onScanEvent);

    return () => {
      socket.off("connect", onConnect);
      socket.off("disconnect", onDisconnect);
      socket.off("scan", onScanEvent);
    };
  }, []);

  return (
    <main className={`flex flex-col items-center py-16`}>
      <div className="w-[1000px]">
        <h1 className="text-4xl font-bold text-center">ARTEMIS</h1>
        <div className="mt-10 flex items-center justify-between">
          <ConnectionState isConnected={isConnected} />
          <ConnectionManager />
        </div>
        <Separator className="my-4" />
        <MyForm />

        <List list={list} />
      </div>
    </main>
  );
}

export default App;
