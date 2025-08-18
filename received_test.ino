#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define led 2
#define QUEUE_SIZE 10

// Cấu trúc dữ liệu nhận
typedef struct struct_message{
  char mac[250];
  char qrcode[250];
}struct_message;

struct_message myData;

// Khai báo Queue và Semaphore
QueueHandle_t dataQueue;
SemaphoreHandle_t dataSemaphore;

// Task handles
TaskHandle_t receiveTaskHandle = NULL;
TaskHandle_t processTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;

// Callback nhận dữ liệu ESP-NOW
void OnDataRecv(const uint8_t *macAddr, const uint8_t *data, int len){
  struct_message receivedData;
  memcpy(&receivedData, data, len);
  
  // Gửi dữ liệu vào queue (không in thông báo)
  xQueueSend(dataQueue, &receivedData, 0);
}

// Task 1: Nhận và in dữ liệu từ queue
void receiveDataTask(void *parameter){
  struct_message receivedData;
  
  while(1){
    // Đợi dữ liệu từ queue
    if(xQueueReceive(dataQueue, &receivedData, portMAX_DELAY) == pdTRUE){
      // Chỉ in 2 dòng: MAC và QR Code
      Serial.printf("MAC: %s\n", receivedData.mac);
      Serial.printf("QR Code: %s\n", receivedData.qrcode);
      
      // Báo hiệu có dữ liệu mới cho task xử lý
      xSemaphoreGive(dataSemaphore);
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task 2: Xử lý dữ liệu
void processDataTask(void *parameter){
  while(1){
    // Đợi semaphore từ receive task
    if(xSemaphoreTake(dataSemaphore, portMAX_DELAY) == pdTRUE){
      // Xử lý dữ liệu im lặng (không in gì)
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

// Task 3: Điều khiển LED
void ledControlTask(void *parameter){
  bool ledState = false;
  
  while(1){
    // Kiểm tra nếu có dữ liệu trong queue
    if(uxQueueMessagesWaiting(dataQueue) > 0){
      // Nhấp nháy LED nhanh khi có dữ liệu
      digitalWrite(led, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(led, LOW);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    } else {
      // Nhấp nháy LED chậm khi không có dữ liệu
      ledState = !ledState;
      digitalWrite(led, ledState);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  
  Serial.println("Initializing ESP-NOW with FreeRTOS...");
  
  // Cấu hình WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.println("WiFi MAC Address: " + WiFi.macAddress());
  
  // Khởi tạo ESP-NOW
  if(esp_now_init() != ESP_OK){
    Serial.println("ESP-NOW initialization failed!");
    // Nhấp nháy LED báo lỗi
    for(int i = 0; i < 10; i++){
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
      delay(100);
    }
    return;
  }
  
  Serial.println("ESP-NOW initialized successfully");
  
  // Đăng ký callback nhận dữ liệu
  esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
  
  // Tạo Queue và Semaphore
  dataQueue = xQueueCreate(QUEUE_SIZE, sizeof(struct_message));
  dataSemaphore = xSemaphoreCreateBinary();
  
  if(dataQueue == NULL || dataSemaphore == NULL){
    Serial.println("Failed to create Queue or Semaphore!");
    return;
  }
  
  Serial.println("Queue and Semaphore created successfully");
  
  // Tạo các FreeRTOS Tasks
  // Task 1: Receive Data (Priority cao)
  xTaskCreate(
    receiveDataTask,      // Task function
    "ReceiveTask",        // Task name
    2048,                 // Stack size
    NULL,                 // Parameters
    3,                    // Priority (cao)
    &receiveTaskHandle    // Task handle
  );
  
  // Task 2: Process Data (Priority trung bình)
  xTaskCreate(
    processDataTask,      // Task function
    "ProcessTask",        // Task name
    2048,                 // Stack size
    NULL,                 // Parameters
    2,                    // Priority (trung bình)
    &processTaskHandle    // Task handle
  );
  
  // Task 3: LED Control (Priority thấp)
  xTaskCreate(
    ledControlTask,       // Task function
    "LEDTask",           // Task name
    1024,                // Stack size (nhỏ hơn)
    NULL,                // Parameters
    1,                   // Priority (thấp)
    &ledTaskHandle       // Task handle
  );
  
  Serial.println("All FreeRTOS tasks created successfully");
  Serial.println("System ready to receive ESP-NOW data...");
}

void loop() {
  // Loop trống - tất cả công việc được xử lý bởi FreeRTOS tasks
  // FreeRTOS scheduler sẽ quản lý các tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
