#include "M5UnitQRCode.h"
#include <esp_now.h>
#include <WiFi.h>

M5UnitQRCodeUART qrcode;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Struct phải khớp với receiver
typedef struct struct_message {
  char mac[18];      // MAC address (17 chars + null terminator)
  char qrcode[32];   // QR code data
  uint32_t timestamp; // Sender timestamp
  uint16_t sequence;  // Sequence number
  uint8_t rssi;      // Signal strength (sẽ được receiver điền)
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Biến để track sequence và tránh spam
uint16_t sequenceNumber = 0;
String lastQRCode = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000; // Gửi mỗi 1 giây tối đa

void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("📡 Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Success" : "❌ Failed");
}

void setup() {
  Serial.begin(115200);
  Serial.println("🚀 ESP32 QR Code Sender Starting...");
  
  WiFi.mode(WIFI_STA);
  Serial.print("📶 MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Khởi tạo QR Code reader
  while (!qrcode.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 16, 17)) {
    Serial.println("❌ Unit QRCode UART Init Failed, retrying...");
    delay(1000);
  }
  Serial.println("✅ Unit QRCode UART Initialized");
  qrcode.setTriggerMode(AUTO_SCAN_MODE);

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
  Serial.println("✅ ESP-NOW initialized");

  // Đăng ký peer (broadcast)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }
  
  Serial.println("✅ Broadcast peer added");
  Serial.println("🎯 Ready to scan QR codes...\n");
}

void sendQRData(String qrData) {
  // Chuẩn bị dữ liệu
  memset(&myData, 0, sizeof(myData)); // Clear struct
  
  // Copy MAC address (chỉ lấy 17 chars)
  String macStr = WiFi.macAddress();
  strncpy(myData.mac, macStr.c_str(), sizeof(myData.mac) - 1);
  myData.mac[sizeof(myData.mac) - 1] = '\0';
  
  // Copy QR code data
  strncpy(myData.qrcode, qrData.c_str(), sizeof(myData.qrcode) - 1);
  myData.qrcode[sizeof(myData.qrcode) - 1] = '\0';
  
  // Thêm timestamp và sequence
  myData.timestamp = millis();
  myData.sequence = ++sequenceNumber;
  myData.rssi = 0; // Sẽ được receiver điền
  
  // Gửi dữ liệu
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  
  if (result == ESP_OK) {
    Serial.printf("📤 Sent QR: %s | SEQ: %d | TS: %lu\n", 
                  myData.qrcode, myData.sequence, myData.timestamp);
  } else {
    Serial.println("❌ Error sending data");
  }
}

void loop() {
  if (qrcode.available()) {
    String data = qrcode.getDecodeData();
    uint16_t length = data.length();
    
    if (length > 0) {
      Serial.printf("📖 QR Detected (len:%d): %s\n", length, data.c_str());
      
      unsigned long currentTime = millis();
      
      // Kiểm tra nếu là QR code mới hoặc đã đủ thời gian từ lần gửi cuối
      if (data != lastQRCode || (currentTime - lastSendTime) >= SEND_INTERVAL) {
        
        sendQRData(data);
        
        lastQRCode = data;
        lastSendTime = currentTime;
      } else {
        Serial.println("⏳ Same QR code, waiting for interval...");
      }
    }
  }
  
  delay(100); // Tránh spam quá nhanh
}