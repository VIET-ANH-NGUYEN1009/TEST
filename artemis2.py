import time
import serial
import requests
import json

# Sửa lại dấu cách thừa
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200
API_URL = 'http://10.0.108.10:8387/products'  # Cập nhật lại URL API nếu cần
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

last_qr_data = ""
#print("aaaa")

while True:
    try:
        #print("aaaa")
        line = ser.readline().decode('utf-8').rstrip()
        
        
        if line and line != last_qr_data:
            last_qr_data = line
            print("Received data from ESP32: ", line) 
            
            payload = {'id': line}
            headers={"Content-Type":"application/json"}

            # response = requests.get(API_URL)
            # response = requests.put(API_URL, data=json.dumps(payload),headers=headers)
            # response = requests.delete(f"{API_URL}/{qr_id}", headers=headers)
            response = requests.post(API_URL, data=json.dumps(payload),headers=headers)
            print("Response:", response.status_code, response.text)
            
    
    except Exception as e:
        print("ERROR: ", e)
        time.sleep(1)


