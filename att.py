import serial
import json
from datetime import datetime
import tkinter as tk
from tkinter import ttk
import threading
import queue
import webbrowser
import socketio

# --- Cấu hình serial ---
SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
BAUD_RATE = 115200

data_queue = queue.Queue()
records = []
last_qr_code = None

# --- Socket.IO client ---
sio = socketio.Client()

@sio.event
def connect():
    print("Connected to WebSocket server")

@sio.event
def disconnect():
    print("Disconnected from WebSocket server")

# Kết nối tới server WebSocket
sio.connect('http://10.0.108.10:99', socketio_path='/api.artemis/socket')


# --- Thread đọc serial ---
def serial_thread():
    global last_qr_code
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        mac_address = ""
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').rstrip()
                print(datetime.now().strftime("[%Y-%m-%d %H:%M:%S]"), "Raw line:", line)

                if line.startswith("mac:"):
                    mac_address = line.split("mac:")[1].strip()
                    print("MAC:", mac_address)

                elif line.startswith("Qr Code:"):
                    qr_code = line.split("Qr Code:")[1].strip()
                    print("QR Code:", qr_code)

                    if qr_code and qr_code != last_qr_code:
                        last_qr_code = qr_code
                        # Gửi QR code lên server WebSocket
                        sio.emit('scan', {'qr_code': qr_code, 'user_id': 'ARTEMIS'})
                        # Đẩy MAC + QR vào queue để hiển thị trên GUI
                        data_queue.put((mac_address, qr_code))
    except Exception as e:
        print("Serial thread error:", e)
    finally:
        ser.close()
        print("Serial closed")


# --- Cập nhật GUI ---
def update_gui():
    while not data_queue.empty():
        mac, qr = data_queue.get()
        records.append((mac, qr))
        if len(records) > 10:
            records.pop(0)

    for i in tree.get_children():
        tree.delete(i)
    for mac, qr in records:
        tree.insert("", "end", values=(mac, qr))

    root.after(500, update_gui)


# --- GUI ---
root = tk.Tk()
root.title("Artemis")

tree = ttk.Treeview(root, columns=("MAC", "QR Code"), show="headings")
tree.heading("MAC", text="MAC Address")
tree.heading("QR Code", text="QR Code")
tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)


def open_artemis():
    webbrowser.open("http://10.0.108.10/artemis/")


btn_open = tk.Button(root, text="Artemis System", command=open_artemis)
btn_open.pack(pady=(0, 10))

# --- Start thread và GUI ---
threading.Thread(target=serial_thread, daemon=True).start()
update_gui()

root.mainloop()
