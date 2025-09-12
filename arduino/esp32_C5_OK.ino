#include <Arduino.h>
#include <Wire.h>
#include "M5UnitQRCode.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

// ===== QR Modules =====
M5UnitQRCodeI2C qrI2C;
M5UnitQRCodeUART qrUART;

// ===== WiFi / WebSocket =====
const char* ssid = "JIG-ASSY";
const char* password = "12345678901234567890123456";
const char* ws_server = "10.0.108.10";
const uint16_t ws_port = 99;
const char* ws_path = "/api.artemis/socket";
WebSocketsClient webSocket;

// ===== Queue =====
#define QR_QUEUE_SIZE 10
#define MAX_QR_LEN 100
QueueHandle_t qrQueue;

// ===== Struct gửi queue =====
typedef struct {
  char qr[MAX_QR_LEN];
} QRItem;

// ===== Kiểm tra QR hợp lệ =====
bool isValidQR(const char* qr) {
  return qr[0] != '\0';
}

// ===== WebSocket callback =====
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED: Serial.println("WebSocket connected"); break;
    case WStype_DISCONNECTED: Serial.println("WebSocket disconnected"); break;
    case WStype_TEXT: Serial.printf("Received: %s\n", payload); break;
    default: break;
  }
}

// ===== Task QR =====
void QRTask(void * pvParameters) {
  char lastI2CQR[MAX_QR_LEN] = "";
  char lastUARTQR[MAX_QR_LEN] = "";
  char qrBuffer[MAX_QR_LEN];

  for (;;) {
    // --- I2C ---
    if (qrI2C.getDecodeReadyStatus() == 1) {
      uint16_t len = qrI2C.getDecodeLength();
      if (len > 0 && len < MAX_QR_LEN) {
        qrI2C.getDecodeData((uint8_t*)qrBuffer, len);
        qrBuffer[len] = '\0';
        if (isValidQR(qrBuffer) && strcmp(qrBuffer, lastI2CQR) != 0) {
          Serial.print("[I2C] QR: "); Serial.println(qrBuffer);

          QRItem item;
          strncpy(item.qr, qrBuffer, MAX_QR_LEN);
          xQueueSend(qrQueue, &item, portMAX_DELAY);

          strcpy(lastI2CQR, qrBuffer);
        }
      }
    }

    // --- UART ---
    if (qrUART.available()) {
      String qrStr = qrUART.getDecodeData();
      qrStr.toCharArray(qrBuffer, MAX_QR_LEN);
      if (isValidQR(qrBuffer) && strcmp(qrBuffer, lastUARTQR) != 0) {
        Serial.print("[UART] QR: "); Serial.println(qrBuffer);

        QRItem item;
        strncpy(item.qr, qrBuffer, MAX_QR_LEN);
        xQueueSend(qrQueue, &item, portMAX_DELAY);

        strcpy(lastUARTQR, qrBuffer);
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ===== Task WebSocket =====
void WSTask(void * pvParameters) {
  QRItem item;
  unsigned long lastReconnectAttempt = 0;

  for (;;) {
    webSocket.loop();

    unsigned long now = millis();
    if (!webSocket.isConnected() && now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      Serial.println("Reconnecting WebSocket...");
      webSocket.begin(ws_server, ws_port, ws_path);
    }

    while (xQueueReceive(qrQueue, &item, 0) == pdTRUE) {
      if (webSocket.isConnected()) {
        // Tạo JSON để gửi backend
        String payload = String("{\"qr_code\":\"") + item.qr + "\",\"user_id\":\"ESP32\"}";
        webSocket.sendTXT(payload);
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- WiFi ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());

  // --- WebSocket ---
  webSocket.begin(ws_server, ws_port, ws_path);
  webSocket.onEvent(webSocketEvent);

  // --- QR Modules ---
  Wire.begin(2,3);
  qrI2C.begin(&Wire, UNIT_QRCODE_ADDR, 2, 3, 100000U);
  qrI2C.setTriggerMode(AUTO_SCAN_MODE);
  qrUART.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 4, 5);
  qrUART.setTriggerMode(AUTO_SCAN_MODE);

  // --- Queue ---
  qrQueue = xQueueCreate(QR_QUEUE_SIZE, sizeof(QRItem));

  // --- FreeRTOS tasks ---
  xTaskCreate(QRTask, "QR Task", 8192, NULL, 1, NULL);
  xTaskCreate(WSTask, "WS Task", 8192, NULL, 2, NULL);
}

void loop() {
  // Không cần loop, FreeRTOS chạy task song song
}
