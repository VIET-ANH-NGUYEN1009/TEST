import serial
import requests
import json
from datetime import datetime
import tkinter as tk
from tkinter import ttk
import threading
import queue
import webbrowser

SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
BAUD_RATE = 115200
API_URL = 'http://10.0.108.10:8387/products'

headers = {"Content-Type": "application/json"}
last_qr_code = None

data_queue = queue.Queue()


records = []

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
                        payload = {"qr": qr_code}
                        try:
                            response = requests.post(API_URL, data=json.dumps(payload), headers=headers, timeout=3)
                            print("Response:", response.status_code, response.text)
                        except requests.RequestException as e:
                            print("Error API:", e)


                        data_queue.put((mac_address, qr_code))
    except Exception as e:
        print("Serial thread error:", e)
    finally:
        ser.close()
        print("Serial closed")

def update_gui():
    # Lấy dữ liệu mới từ queue
    while not data_queue.empty():
        mac, qr = data_queue.get()
        records.append((mac, qr))
        # Giữ tối đa 10 hàng
        if len(records) > 10:
            records.pop(0)

    # Xóa tất cả item và update Treeview
    for i in tree.get_children():
        tree.delete(i)
    for mac, qr in records:
        tree.insert("", "end", values=(mac, qr))

    root.after(500, update_gui)  

# --- GUI ---
root = tk.Tk()
root.title("Artemis")

# Treeview hiển thị MAC + QR
tree = ttk.Treeview(root, columns=("MAC", "QR Code"), show="headings")
tree.heading("MAC", text="MAC Address")
tree.heading("QR Code", text="QR Code")
tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)


def open_artemis():
    webbrowser.open("http://10.0.108.10:8387/artemis")

btn_open = tk.Button(root, text="Artemis System", command=open_artemis)
btn_open.pack(pady=(0, 10))


threading.Thread(target=serial_thread, daemon=True).start()
update_gui()

root.mainloop()
