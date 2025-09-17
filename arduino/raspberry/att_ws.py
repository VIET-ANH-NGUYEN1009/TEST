import serial  
import tkinter as tk  
from tkinter import ttk  
import threading  
import queue  
import webbrowser  
import sys  
import time  
import socket  
import asyncio  
import websockets  
import json  

# --- serial ---  
SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_8467944d74d6ef11a55f694b49d2c684-if00-port0'  
SERIAL_PORT1 = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'  
BAUD_RATE = 115200  

# --- WebSocket URL (raw WS endpoint on your server) ---  
WS_URL = 'ws://10.0.108.10:99/api.artemis/ws'  

# Queues  
data_queue = queue.Queue()     # for GUI updates  
ws_send_queue = queue.Queue()  # for messages to send over WebSocket  
records = []  

# --- Wait for network ---  
def wait_for_network(host="10.0.108.10", port=99, timeout=1):  
    while True:  
        try:  
            with socket.create_connection((host, port), timeout=timeout):  
                print(" Network ready")  
                return  
        except OSError:  
            print(" Waiting for network...")  
            time.sleep(2)  

# --- Serial đọc thread (giữ logic của bạn) ---  
def serial_thread(port):  
    last_qr_code = None  
    ser = None  
    try:  
        ser = serial.Serial(port, BAUD_RATE, timeout=1)  
        user_id = ""  
        print(f" Serial thread started for {port}")  
        while True:  
            if ser.in_waiting > 0:  
                line = ser.readline().decode(errors="ignore").strip()  

                if line.startswith("User ID:"):  
                    user_id = line.split("User ID:")[1].strip()  
                    print(f"[{port}] User ID: {user_id}")  

                elif line.startswith("QR Code:"):  
                    qr_code = line.split("QR Code:")[1].strip()  
                    print(f"[{port}] QR Code: {qr_code}")  

                    if qr_code and qr_code != last_qr_code:  
                        last_qr_code = qr_code  

                        # Prepare payload expected by server's WS handler  
                        payload = {'qr_code': qr_code, 'user_id': user_id}  
                        try:  
                            ws_send_queue.put_nowait(json.dumps(payload))  
                            print(f"[{port}] Queued to send via WebSocket")  
                        except Exception as e:  
                            print("Queue send error:", e)  

                        data_queue.put((port, user_id, qr_code))  
            else:  
                time.sleep(0.05)  
    except Exception as e:  
        print(f"Serial thread error ({port}):", e)  
    finally:  
        try:  
            if ser:  
                ser.close()  
                print(f"Serial closed ({port})")  
        except:  
            pass  

# --- Async WebSocket handler (chạy trong 1 thread thông qua asyncio.run) ---  
async def ws_sender(ws):  
    while True:  
        # Blocking queue.get run in thread to avoid blocking event loop  
        msg = await asyncio.to_thread(ws_send_queue.get)  
        try:  
            await ws.send(msg)  
            print("Sent to server:", msg)  
        except Exception as e:  
            print("Error sending message over WebSocket:", e)  
            # Put back for retry  
            try:  
                ws_send_queue.put_nowait(msg)  
            except:  
                pass  
            raise  

async def ws_receiver(ws):  
    while True:  
        try:  
            msg = await ws.recv()  
            print("Received from server:", msg)  
            # Optionally parse and update GUI if server broadcasts scan  
            # data = json.loads(msg
