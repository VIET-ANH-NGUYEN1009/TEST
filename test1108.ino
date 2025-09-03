#include <esp_now.h>
#include <WiFi.h>
#include "M5UnitQRCode.h"
//d0,cf,13,e0,dd,50
// ================== Cấu hình ESP-NOW ==================
//uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t broadcastAddress[] = {0xd0, 0xcf, 0x13, 0xe0, 0xdd, 0x50};


// Cấu trúc dữ liệu gửi
typedef struct struct_message {
  char a[32];  // chứa dữ liệu QR
  int b;       // ví dụ: độ dài QR
  float c;     // có thể dùng sau này
  bool d;      // có thể dùng sau này
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Callback khi gửi xong
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// ================== Cấu hình QR Code ==================
M5UnitQRCodeUART qrcode;

void setup() {
  Serial.begin(115200);

  // Khởi tạo QR Code qua UART2
  while (!qrcode.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 16, 17)) {
    Serial.println("Unit QRCode UART Init Fail");
    delay(1000);
  }
  Serial.println("Unit QRCode UART Init Success");
  qrcode.setTriggerMode(AUTO_SCAN_MODE);

  // Cấu hình WiFi ở chế độ Station
  WiFi.mode(WIFI_STA);

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Đăng ký callback gửi
  esp_now_register_send_cb(OnDataSent);

  // Thêm peer (broadcast)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  // Nếu có dữ liệu QR mới
  if (qrcode.available()) {
    String data = qrcode.getDecodeData();
    Serial.printf("len:%d\r\n", data.length());
    Serial.printf("decode data: %s\r\n", data.c_str());

    // Gán dữ liệu vào struct để gửi qua ESP-NOW
    strncpy(myData.a, data.c_str(), sizeof(myData.a) - 1);
    myData.a[sizeof(myData.a) - 1] = '\0'; // đảm bảo null-terminated
    myData.b = data.length();
    myData.c = 0;  // chưa dùng
    myData.d = true; // đánh dấu có dữ liệu mới

    // Gửi qua ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    } else {
      Serial.println("Error sending the data");
    }
  }
}
