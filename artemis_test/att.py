import serial
import json
from datetime import datetime
import threading
import socketio
import time

# --- Cấu hình serial ---
SERIAL_PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
BAUD_RATE = 115200
last_qr_code = None
records = []
is_running = True

# --- Socket.IO client ---
sio = socketio.Client()

@sio.event
def connect():
    print(f"[{datetime.now().strftime('%H:%M:%S')}]  Connected to WebSocket server")

@sio.event
def disconnect():
    print(f"[{datetime.now().strftime('%H:%M:%S')}]  Disconnected from WebSocket server")

# Kết nối tới server WebSocket
try:
    sio.connect('http://10.0.108.10:99', socketio_path='/api.artemis/socket')
except Exception as e:
    print(f"Failed to connect to WebSocket: {e}")

# --- Thread đọc serial ---
def serial_thread():
    global last_qr_code, is_running
    
    while is_running:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            print(f"[{datetime.now().strftime('%H:%M:%S')}]  Connected to serial port")
            
            mac_address = ""
            
            while is_running:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8').rstrip()
                        
                        if line.startswith("MAC:"):
                            mac_address = line.split("MAC:")[1].strip()
                            print(f"[{datetime.now().strftime('%H:%M:%S')}]  MAC: {mac_address}")
                            
                        elif line.startswith("QR Code:"):
                            qr_code = line.split("QR Code:")[1].strip()
                            
                            if qr_code and qr_code != last_qr_code:
                                last_qr_code = qr_code
                                timestamp = datetime.now().strftime('%H:%M:%S')
                                
                                print(f"[{timestamp}]  QR Code: {qr_code}")
                                
                                # Gửi QR code lên server WebSocket
                                try:
                                    if sio.connected:
                                        sio.emit('scan', {'qr_code': qr_code, 'user_id': 'ARTEMIS'})
                                        print(f"[{timestamp}]  Sent to server: {qr_code}")
                                    else:
                                        print(f"[{timestamp}]   WebSocket not connected, data not sent")
                                except Exception as e:
                                    print(f"[{timestamp}]  Failed to send to server: {e}")
                                
                                # Lưu vào records
                                records.append({
                                    'timestamp': datetime.now(),
                                    'mac': mac_address,
                                    'qr': qr_code
                                })
                                
                                # Giới hạn số records
                                if len(records) > 100:
                                    records.pop(0)
                    
                    except UnicodeDecodeError:
                        continue
                
                time.sleep(0.01)  # Tránh CPU cao
                
        except serial.SerialException as e:
            print(f"[{datetime.now().strftime('%H:%M:%S')}]  Serial error: {e}")
            print(" Retrying in 5 seconds...")
            time.sleep(5)
            
        except Exception as e:
            print(f"[{datetime.now().strftime('%H:%M:%S')}]  Unexpected error: {e}")
            time.sleep(5)
        
        finally:
            try:
                ser.close()
            except:
                pass

# --- Status thread ---
def status_thread():
    global is_running
    
    while is_running:
        time.sleep(30)  # Hiển thị status mỗi 30 giây
        if records:
            print(f"\n📊 Status: {len(records)} records, WebSocket: {'ok' if sio.connected else 'ng'}")
            print(f"   Last scan: {records[-1]['qr'][:30]}...")
            print("-" * 50)

# --- Main function ---
def main():
    global is_running

    print(" Serial Port:", SERIAL_PORT)
    print(" WebSocket URL: http://10.0.108.10:99/api.artemis/socket")
    print("Press Ctrl+C to stop\n")
    
    # Start threads
    serial_t = threading.Thread(target=serial_thread, daemon=True)
    status_t = threading.Thread(target=status_thread, daemon=True)
    
    serial_t.start()
    status_t.start()
    
    try:
        # Chạy vô hạn cho đến khi Ctrl+C
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n Stopping scanner...")
        is_running = False
        
        # Disconnect WebSocket
        if sio.connected:
            sio.disconnect()
        
        print(" Scanner stopped")

if __name__ == "__main__":
    main()