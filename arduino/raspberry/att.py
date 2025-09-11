import serial
import tkinter as tk
from tkinter import ttk
import threading
import queue
import webbrowser
import socketio
import sys
import time
import socket

# --- Cấu hình serial ---
SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_8467944d74d6ef11a55f694b49d2c684-if00-port0'
SERIAL_PORT1 = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
BAUD_RATE = 115200

data_queue = queue.Queue()
records = []

# --- Socket.IO client ---
sio = socketio.Client()

@sio.event
def connect():
    print("✅ Connected to WebSocket server")

@sio.event
def disconnect():
    print("❌ Disconnected from WebSocket server")

# --- Wait for network ---
def wait_for_network(host="10.0.108.10", port=99, timeout=1):
    while True:
        try:
            with socket.create_connection((host, port), timeout=timeout):
                print("🌐 Network ready")
                return
        except OSError:
            print("⏳ Waiting for network...")
            time.sleep(2)

# --- Connect with retry ---
def connect_socketio():
    wait_for_network()
    connected = False
    while not connected:
        try:
            sio.connect('http://10.0.108.10:99', socketio_path='/api.artemis/socket')
            connected = True
        except Exception as e:
            print("Socket.IO connect error:", e)
            time.sleep(5)

# --- Thread đọc serial ---
def serial_thread(port):
    last_qr_code = None
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        user_id = ""
        print(f"✅ Serial thread started for {port}")
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode(errors="ignore").strip()

                if line.startswith("User ID:"):
                    user_id = line.split("User ID:")[1].strip()
                    print(f"[{port}] User ID:", user_id)

                elif line.startswith("QR Code:"):
                    qr_code = line.split("QR Code:")[1].strip()
                    print(f"[{port}] QR Code:", qr_code)

                    if qr_code and qr_code != last_qr_code:
                        last_qr_code = qr_code
                        try:
                            sio.emit('scan', {'qr_code': qr_code, 'user_id': user_id})
                            print(f"[{port}] Sent to server")
                        except Exception as e:
                            print("Send error:", e)

                        data_queue.put((port, user_id, qr_code))
    except Exception as e:
        print(f"Serial thread error ({port}):", e)
    finally:
        try:
            ser.close()
            print(f"Serial closed ({port})")
        except:
            pass

# --- Cập nhật GUI ---
def update_gui():
    while not data_queue.empty():
        port, uid, qr = data_queue.get()
        records.append((port, uid, qr))
        if len(records) > 10:
            records.pop(0)

    for i in tree.get_children():
        tree.delete(i)
    for port, uid, qr in records:
        tree.insert("", "end", values=(port, uid, qr))

    root.after(500, update_gui)

# --- GUI ---
root = tk.Tk()
root.title("Artemis - QR Scanner")
root.geometry("650x300")

tree = ttk.Treeview(root, columns=("Port", "UserId", "QR Code"), show="headings")
tree.heading("Port", text="Serial Port")
tree.heading("UserId", text="User ID")
tree.heading("QR Code", text="QR Code")
tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

def open_artemis():
    webbrowser.open("http://10.0.108.10/artemis/")

btn_open = tk.Button(root, text="Open Artemis System", command=open_artemis)
btn_open.pack(pady=(0, 10))

# --- Start Socket.IO thread ---
threading.Thread(target=connect_socketio, daemon=True).start()

# --- Start serial threads ---
threading.Thread(target=serial_thread, args=(SERIAL_PORT,), daemon=True).start()
threading.Thread(target=serial_thread, args=(SERIAL_PORT1,), daemon=True).start()

# --- Bắt đầu GUI ---
update_gui()

def on_closing():
    try:
        sio.disconnect()
    except:
        pass
    root.destroy()
    sys.exit()

root.protocol("WM_DELETE_WINDOW", on_closing)
root.mainloop()
