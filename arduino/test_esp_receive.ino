#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <ArduinoJson.h>

// ========== Data Structure (phải giống với sender) ==========
#define MAX_QR_LEN 100
#define QUEUE_SIZE 10
#define LED_PIN 2

typedef struct {
  char qrCode[MAX_QR_LEN];
  char userId[16];
  uint32_t timestamp;
  uint8_t checksum;
} QRMessage;

// ========== Global Variables ==========
QueueHandle_t receivedQueue;
SemaphoreHandle_t dataMutex;
unsigned long lastReceiveTime = 0;
int totalReceived = 0;

// ========== Utility Functions ==========
uint8_t calculateChecksum(const char* data) {
  uint8_t checksum = 0;
  for (int i = 0; i < strlen(data); i++) {
    checksum ^= data[i];
  }
  return checksum;
}

bool validateMessage(const QRMessage* msg) {
  // Kiểm tra độ dài QR code
  if (strlen(msg->qrCode) == 0 || strlen(msg->qrCode) >= MAX_QR_LEN) {
    return false;
  }
  
  // Kiểm tra checksum
  uint8_t calculatedChecksum = calculateChecksum(msg->qrCode);
  if (calculatedChecksum != msg->checksum) {
    Serial.println("⚠️ Checksum mismatch!");
    return false;
  }
  
  return true;
}

// ========== ESP-NOW Callback ==========
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(QRMessage)) {
    QRMessage receivedMsg;
    memcpy(&receivedMsg, incomingData, sizeof(receivedMsg));
    
    // LED blink để báo hiệu nhận được data
    digitalWrite(LED_PIN, HIGH);
    
    // Gửi vào queue để xử lý
    if (xQueueSend(receivedQueue, &receivedMsg, 0) == pdTRUE) {
      totalReceived++;
      lastReceiveTime = millis();
    } else {
      Serial.println("⚠️ Queue full - Data dropped!");
    }
    
    // Tắt LED sau 100ms
    vTaskDelay(pdMS_TO_TICKS(100));
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.printf("❌ Invalid data size received: %d bytes\n", len);
  }
}

// ========== Task xử lý dữ liệu nhận được ==========
void TaskProcessReceived(void *pvParameters) {
  QRMessage receivedMsg;
  
  while (1) {
    // Đợi dữ liệu từ queue
    if (xQueueReceive(receivedQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) {
      
      // Validate dữ liệu
      if (validateMessage(&receivedMsg)) {
        
        // In thông tin nhận được
        Serial.println("📥 ===== NEW QR RECEIVED =====");
        Serial.printf("🔍 QR Code: %s\n", receivedMsg.qrCode);
        Serial.printf("👤 User ID: %s\n", receivedMsg.userId);
        Serial.printf("⏰ Timestamp: %lu\n", receivedMsg.timestamp);
        Serial.printf("🔐 Checksum: 0x%02X\n", receivedMsg.checksum);
        Serial.printf("📊 Total Received: %d\n", totalReceived);
        Serial.printf("⏱️  Receive Time: %lu ms\n", millis());
        Serial.println("================================\n");
        
        // TODO: Thêm logic xử lý QR code tại đây
        processQRCode(receivedMsg.qrCode, receivedMsg.userId, receivedMsg.timestamp);
        
      } else {
        Serial.println("❌ Invalid message received!");
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ========== Hàm xử lý QR Code ==========
void processQRCode(const char* qrCode, const char* userId, uint32_t timestamp) {
  // TODO: Thêm logic xử lý cụ thể cho QR code
  
  // Ví dụ: Kiểm tra loại QR code
  String qrStr = String(qrCode);
  
  if (qrStr.startsWith("http")) {
    Serial.printf("🌐 URL detected: %s\n", qrCode);
    // Xử lý URL
  } 
  else if (qrStr.startsWith("WIFI:")) {
    Serial.printf("📶 WiFi QR detected: %s\n", qrCode);
    // Xử lý WiFi credential
  }
  else if (qrStr.indexOf("@") != -1) {
    Serial.printf("📧 Email detected: %s\n", qrCode);
    // Xử lý email
  }
  else if (qrStr.length() >= 10 && qrStr.length() <= 15) {
    bool isNumeric = true;
    for (int i = 0; i < qrStr.length(); i++) {
      if (!isdigit(qrStr[i]) && qrStr[i] != '+' && qrStr[i] != '-' && qrStr[i] != ' ') {
        isNumeric = false;
        break;
      }
    }
    if (isNumeric) {
      Serial.printf("📞 Phone number detected: %s\n", qrCode);
      // Xử lý số điện thoại
    }
  }
  else {
    Serial.printf("📝 Text/Other: %s\n", qrCode);
    // Xử lý text khác
  }
  
  // Log vào file hoặc gửi lên server
  logQRCode(qrCode, userId, timestamp);
}

// ========== Log QR Code ==========
void logQRCode(const char* qrCode, const char* userId, uint32_t timestamp) {
  // TODO: Lưu vào SPIFFS, SD Card hoặc gửi lên server
  
  // Tạo JSON log
  StaticJsonDocument<200> doc;
  doc["qrCode"] = qrCode;
  doc["userId"] = userId;
  doc["timestamp"] = timestamp;
  doc["receivedAt"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.printf("📝 Log: %s\n", jsonString.c_str());
  
  // TODO: Gửi lên server HTTP/MQTT
  // sendToServer(jsonString);
}

// ========== Task Monitor System ==========
void TaskMonitor(void *pvParameters) {
  unsigned long lastPrintTime = 0;
  
  while (1) {
    unsigned long currentTime = millis();
    
    // In thống kê mỗi 30 giây
    if (currentTime - lastPrintTime > 30000) {
      Serial.println("📊 ===== SYSTEM STATUS =====");
      Serial.printf("📈 Total Received: %d\n", totalReceived);
      Serial.printf("📶 WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      Serial.printf("🆓 Free Heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("⏰ Uptime: %lu ms\n", currentTime);
      
      if (lastReceiveTime > 0) {
        Serial.printf("📥 Last Receive: %lu ms ago\n", currentTime - lastReceiveTime);
      } else {
        Serial.println("📥 No data received yet");
      }
      
      Serial.println("============================\n");
      lastPrintTime = currentTime;
    }
    
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n🚀 ESP32 QR Code Receiver Starting...");
  
  // Init LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // ========== Init WiFi & ESP-NOW ==========
  Serial.print("🔧 Initializing ESP-NOW... ");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed!");
    ESP.restart();
  }
  
  // Register callback cho receive
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("✅ Success");
  Serial.printf("📡 MAC Address: %s\n", WiFi.macAddress().c_str());
  
  // ========== Init FreeRTOS ==========
  Serial.print("🔧 Initializing FreeRTOS... ");
  
  // Tạo Queue
  receivedQueue = xQueueCreate(QUEUE_SIZE, sizeof(QRMessage));
  dataMutex = xSemaphoreCreateMutex();
  
  if (!receivedQueue || !dataMutex) {
    Serial.println("❌ Failed to create Queue/Mutex!");
    ESP.restart();
  }
  
  // Tạo Tasks
  xTaskCreate(TaskProcessReceived, "ProcessQR", 4096, NULL, 3, NULL);
  xTaskCreate(TaskMonitor, "Monitor", 2048, NULL, 1, NULL);
  
  Serial.println("✅ Success");
  Serial.println("🎉 System Ready - Waiting for QR codes...\n");
  
  // Blink LED để báo system ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}

// ========== Loop ==========
void loop() {
  // Watchdog reset
  vTaskDelay(pdMS_TO_TICKS(1000));
}
