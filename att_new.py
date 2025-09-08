import serial
import tkinter as tk
from tkinter import ttk
import threading
import queue
import webbrowser
import socketio
import sys

# --- Cấu hình serial ---
SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_8467944d74d6ef11a55f694b49d2c684-if00-port0'
BAUD_RATE = 115200

data_queue = queue.Queue()
records = []
last_qr_code = None

# --- Socket.IO client ---
sio = socketio.Client()

@sio.event
def connect():
    print("✅ Connected to WebSocket server")

@sio.event
def disconnect():
    print("❌ Disconnected from WebSocket server")

try:
    sio.connect('http://10.0.108.10:99', socketio_path='/api.artemis/socket')
except Exception as e:
    print("Socket.IO connect error:", e)


# --- Thread đọc serial ---
def serial_thread():
    global last_qr_code
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        user_id = ""
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode(errors="ignore").strip()

                if line.startswith("userId:"):
                    user_id = line.split("userId:")[1].strip()
                    print("UserId:", user_id)

                elif line.startswith("qrCode:"):
                    qr_code = line.split("qrCode:")[1].strip()
                    print("QR Code:", qr_code)

                    if qr_code and qr_code != last_qr_code:
                        last_qr_code = qr_code
                        # Gửi QR code + user_id lên server WebSocket
                        try:
                            sio.emit('scan', {'qr_code': qr_code, 'user_id': user_id})
                        except Exception as e:
                            print("Send error:", e)
                        # Đẩy UserId + QR vào queue để hiển thị GUI
                        data_queue.put((user_id, qr_code))
    except Exception as e:
        print("Serial thread error:", e)
    finally:
        try:
            ser.close()
            print("Serial closed")
        except:
            pass


# --- Cập nhật GUI ---
def update_gui():
    while not data_queue.empty():
        uid, qr = data_queue.get()
        records.append((uid, qr))
        if len(records) > 10:  # Giữ tối đa 10 record
            records.pop(0)

    # Xóa bảng cũ và thêm dữ liệu mới
    for i in tree.get_children():
        tree.delete(i)
    for uid, qr in records:
        tree.insert("", "end", values=(uid, qr))

    root.after(500, update_gui)


# --- GUI ---
root = tk.Tk()
root.title("Artemis - QR Scanner")
root.geometry("500x300")

tree = ttk.Treeview(root, columns=("UserId", "QR Code"), show="headings")
tree.heading("UserId", text="User ID")
tree.heading("QR Code", text="QR Code")
tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)


def open_artemis():
    webbrowser.open("http://10.0.108.10/artemis/")


btn_open = tk.Button(root, text="Open Artemis System", command=open_artemis)
btn_open.pack(pady=(0, 10))


# --- Start thread và GUI ---
threading.Thread(target=serial_thread, daemon=True).start()
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
