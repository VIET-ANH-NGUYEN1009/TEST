#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h> 
#include "M5UnitQRCode.h"
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ========== QRCode ==========
M5UnitQRCodeI2C qrcodeI2C;
M5UnitQRCodeUART qrcodeUART;

#define SDA_PIN 21
#define SCL_PIN 22
#define MAX_QR_LEN 100
#define QUEUE_SIZE 5
#define led 2

bool i2c_ok = false;
bool uart_ok = false;

// ========== ESP-NOW Data Structure ==========
typedef struct {
  char qrCode[MAX_QR_LEN];
  char userId[16];
  uint32_t timestamp;  
  uint8_t checksum;    
} QRMessage;

// ========== Global Variables ==========
QRMessage sendData;
QueueHandle_t qrQueue;
SemaphoreHandle_t dataMutex;
String lastQRCode = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000; 

uint8_t broadcastAddress[] = {0xcc, 0xdb, 0xa7, 0x31, 0x56, 0x90}; 
esp_now_peer_info_t peerInfo;

// ========== Utility Functions ==========
uint8_t calculateChecksum(const char* data) {
  uint8_t checksum = 0;
  for (int i = 0; i < strlen(data); i++) {
    checksum ^= data[i];
  }
  return checksum;
}

bool isValidQR(const String& qrData) {
  return qrData.length() > 3 && 
         qrData.length() < MAX_QR_LEN-1 &&
         !qrData.startsWith("ERROR");
}

// ========== ESP-NOW Callback ==========

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.printf(" Send Success");
  } else {
    Serial.printf(" Send Failed");
  }
}

// ========== ESP-NOW Send Task ==========
void TaskSendESPNOW(void *pvParameters) {
  QRMessage qrMsg;
  
  while (1) {
    // Đợi dữ liệu từ queue
    if (xQueueReceive(qrQueue, &qrMsg, portMAX_DELAY) == pdTRUE) {
      
      unsigned long currentTime = millis();
      String currentQR = String(qrMsg.qrCode);
      
      if (currentQR != lastQRCode || (currentTime - lastSendTime) > SEND_INTERVAL) {
        
        // Cập nhật timestamp và checksum
        qrMsg.timestamp = currentTime;
        qrMsg.checksum = calculateChecksum(qrMsg.qrCode);
        
       
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&qrMsg, sizeof(qrMsg));
        
        if (result == ESP_OK) {
          Serial.printf("QR: %s | Time: %lu\n", qrMsg.qrCode, currentTime);
          lastQRCode = currentQR;
          lastSendTime = currentTime;
        } else {
          Serial.printf("ESP-NOW Send Error: %d\n", result);
        }
      } else {
        Serial.printf("⏩ Duplicate QR skipped: %s\n", qrMsg.qrCode);
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// ========== TASK I2C ==========
void TaskI2C(void *pvParameters) {
  uint8_t buffer[MAX_QR_LEN];
  TickType_t lastCheck = 0;
  
  while (1) {
    
    if ((xTaskGetTickCount() - lastCheck) >= pdMS_TO_TICKS(100)) {
      lastCheck = xTaskGetTickCount();
      
      if (qrcodeI2C.getDecodeReadyStatus() == 1) {
        uint16_t len = qrcodeI2C.getDecodeLength();
        
        if (len > 0 && len < MAX_QR_LEN) {
          qrcodeI2C.getDecodeData(buffer, len);
          buffer[len] = '\0'; 
          
          String qrString = String((char*)buffer);
          
          // Validate QR data
          if (isValidQR(qrString)) {
            QRMessage qrMsg;
            strncpy(qrMsg.qrCode, qrString.c_str(), MAX_QR_LEN-1);
            qrMsg.qrCode[MAX_QR_LEN-1] = '\0';
            strncpy(qrMsg.userId, "ESP_I2C", sizeof(qrMsg.userId)-1);
            qrMsg.userId[sizeof(qrMsg.userId)-1] = '\0';
            
          
            if (xQueueSend(qrQueue, &qrMsg, 0) != pdTRUE) {
              Serial.println(" Queue full - I2C data dropped");
            }
          }
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// ========== TASK UART ==========
void TaskUART(void *pvParameters) {
  TickType_t lastCheck = 0;
  
  while (1) {
    if ((xTaskGetTickCount() - lastCheck) >= pdMS_TO_TICKS(100)) {
      lastCheck = xTaskGetTickCount();
      
      if (qrcodeUART.available()) {
        String qrString = qrcodeUART.getDecodeData();
        
        if (isValidQR(qrString)) {
          QRMessage qrMsg;
          strncpy(qrMsg.qrCode, qrString.c_str(), MAX_QR_LEN-1);
          qrMsg.qrCode[MAX_QR_LEN-1] = '\0';
          strncpy(qrMsg.userId, "ESP_UART", sizeof(qrMsg.userId)-1);
          qrMsg.userId[sizeof(qrMsg.userId)-1] = '\0';
          
          // Gửi vào queue
          if (xQueueSend(qrQueue, &qrMsg, 0) != pdTRUE) {
            Serial.println("⚠️ Queue full - UART data dropped");
          }
        }
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ========== System Monitor Task ==========
void TaskMonitor(void *pvParameters) {
  while (1) {
    
    digitalWrite(led,HIGH);
    
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n ESP32 QR Reader Starting...");
  pinMode(led,OUTPUT);
  
  Wire.begin(SDA_PIN, SCL_PIN);

  // ========== Init I2C ==========
  Serial.print(" Init QRCode I2C... ");
  if(qrcodeI2C.begin(&Wire, UNIT_QRCODE_ADDR, SDA_PIN, SCL_PIN, 100000U)) {
    Serial.println(" Success");
    qrcodeI2C.setTriggerMode(AUTO_SCAN_MODE);
    i2c_ok = true;
  } else {
    Serial.println(" Failed");
  }

  // ========== Init UART ==========
  Serial.print(" Init QRCode UART... ");
  if(qrcodeUART.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 16, 17)) {
    Serial.println(" Success");
    qrcodeUART.setTriggerMode(AUTO_SCAN_MODE);
    uart_ok = true;
  } else {
    Serial.println(" Failed - Skipping");
  }

  // ========== Init WiFi & ESP-NOW ==========
  Serial.print(" Init ESP-NOW... ");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if(esp_now_init() != ESP_OK) {
    Serial.println(" ESP-NOW Init Failed!");
    ESP.restart();
  }
  
  esp_now_register_send_cb(OnDataSent);
  
  // Add peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if(esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println(" Failed to add peer!");
    ESP.restart();
  }
  
  Serial.println(" Success");
  Serial.printf(" MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf(" Target MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                broadcastAddress[0], broadcastAddress[1], broadcastAddress[2],
                broadcastAddress[3], broadcastAddress[4], broadcastAddress[5]);

  // ========== Init FreeRTOS ==========
  Serial.print(" Init FreeRTOS... ");
  
  // Tạo Queue và Mutex
  qrQueue = xQueueCreate(QUEUE_SIZE, sizeof(QRMessage));
  dataMutex = xSemaphoreCreateMutex();
  
  if (!qrQueue || !dataMutex) {
    Serial.println(" Failed to create Queue/Mutex!");
    ESP.restart();
  }


  xTaskCreate(TaskSendESPNOW, "ESP-NOW", 4096, NULL, 3, NULL);  
  
  if(i2c_ok) {
    xTaskCreate(TaskI2C, "I2C", 3072, NULL, 2, NULL);
  }
  
  if(uart_ok) {
    xTaskCreate(TaskUART, "UART", 3072, NULL, 2, NULL);
  }
  
  xTaskCreate(TaskMonitor, "Monitor", 2048, NULL, 1, NULL);  
  
  Serial.println("Success");
  Serial.println("System Ready - Scanning for QR codes...\n");
}

void loop() {
  // Watchdog reset để tránh crash
  vTaskDelay(pdMS_TO_TICKS(1000));
}
