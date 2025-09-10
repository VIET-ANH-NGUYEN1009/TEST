#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <ArduinoJson.h>

// ========== Data Structure ==========
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
    
    digitalWrite(LED_PIN, HIGH);
    
    // Gửi vào queue để xử lý
    if (xQueueSend(receivedQueue, &receivedMsg, 0) == pdTRUE) {
      totalReceived++;
      lastReceiveTime = millis();
    } else {
      Serial.println(" Queue full - Data dropped!");
    }
    
    
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

    if (xQueueReceive(receivedQueue, &receivedMsg, portMAX_DELAY) == pdTRUE) {
      
      Serial.println("===== NEW QR RECEIVED =====");
      Serial.printf("QR Code: %s\n", receivedMsg.qrCode);
      Serial.printf("User ID: %s\n", receivedMsg.userId);
      Serial.println("================================\n");
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n ESP32 QR Code Receiver Starting...");
  
  // Init LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // ========== Init WiFi & ESP-NOW ==========
  Serial.print(" Initializing ESP-NOW... ");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != ESP_OK) {
    Serial.println(" ESP-NOW Init Failed!");
    ESP.restart();
  }
  
  // Register callback cho receive
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println(" Success");
  Serial.printf(" MAC Address: %s\n", WiFi.macAddress().c_str());
  
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
 
  Serial.println(" System Ready - Waiting for QR codes...\n");
  
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
 
  vTaskDelay(pdMS_TO_TICKS(1000));
}
