#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>  // Added missing include for watchdog

// Cấu hình
#define QUEUE_SIZE 50
#define MAX_SENDERS 100  // Hỗ trợ 100 senders
#define DUPLICATE_TIMEOUT_MS 5000
#define STATS_REPORT_INTERVAL 30000
#define LOG_FILE "/received_data.txt"
#define MAX_LOG_SIZE 100000  // 100KB

// Cấu trúc dữ liệu
typedef struct struct_message {
  char mac[18];      // MAC address (17 chars + null terminator)
  char qrcode[32];   // QR code data
  uint32_t timestamp; // Sender timestamp
  uint16_t sequence;  // Sequence number
  uint8_t rssi;      // Signal strength
} struct_message;

// Cấu trúc cho duplicate detection (tối ưu bộ nhớ)
typedef struct {
  char qrcode[24];     // Giảm xuống 24 chars
  char mac[13];        // MAC rút gọn: "AABBCCDDEEFF"
  uint32_t lastSeen;
  uint16_t sequence;   // Track sequence để detect duplicates tốt hơn
} duplicate_entry_t;

//統計
typedef struct {
  uint32_t packetsReceived;
  uint32_t packetsDropped;
  uint32_t duplicatesFiltered;
  uint32_t processingErrors;
  uint32_t uptime;
  uint8_t queueUsage;
  uint16_t activeSenders;     // Số lượng senders đang hoạt động
  uint32_t lastCleanup;       // Lần cleanup cuối
} stats_t;

// Biến toàn cục
QueueHandle_t dataQueue;
duplicate_entry_t duplicateTable[MAX_SENDERS];
stats_t systemStats = {0};
uint32_t lastStatsReport = 0;
bool loggingEnabled = true;
SemaphoreHandle_t statsMutex;

// Forward declarations
void cleanupDuplicateTable();
bool isDuplicate(const struct_message* msg);
void updateStats();
void logToFile(const struct_message* msg);
void printSystemInfo();

// Callback nhận dữ liệu ESP-NOW
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) {
    if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
      systemStats.processingErrors++;
      xSemaphoreGive(statsMutex);
    }
    return;  // Fixed: Removed the misplaced else if block
  }

  struct_message msg;
  memcpy(&msg, incomingData, sizeof(msg));
  
  // Thêm RSSI từ thông tin nhận
  msg.rssi = info->rx_ctrl->rssi;

  // Đưa vào queue
  if (xQueueSend(dataQueue, &msg, 0) == pdTRUE) {
    if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
      systemStats.packetsReceived++;
      xSemaphoreGive(statsMutex);
    }
  } else {
    if (xSemaphoreTake(statsMutex, 0) == pdTRUE) {
      systemStats.packetsDropped++;
      xSemaphoreGive(statsMutex);
    }
    Serial.println("⚠️ Queue overflow - packet dropped!");
  }
}

// Task xử lý dữ liệu chính
void processTask(void *pvParameters) {
  struct_message rxMsg;
  
  for (;;) {
    if (xQueueReceive(dataQueue, &rxMsg, portMAX_DELAY) == pdTRUE) {
      
      // Kiểm tra duplicate
      if (isDuplicate(&rxMsg)) {
        if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          systemStats.duplicatesFiltered++;
          xSemaphoreGive(statsMutex);
        }
        continue;
      }

      // Xử lý dữ liệu
      Serial.printf("📦 [%s] QR: %s | SEQ: %d | RSSI: %ddBm | TS: %lu\n",
                    rxMsg.mac, rxMsg.qrcode, rxMsg.sequence, 
                    rxMsg.rssi, rxMsg.timestamp);

      // Log to file nếu được enable
      if (loggingEnabled && SPIFFS.totalBytes() > 0) {
        logToFile(&rxMsg);
      }

      // TODO: Thêm xử lý custom tại đây
      // - Gửi lên server
      // - Xử lý business logic
      // - Trigger actions
      
      // Nhường CPU cho các task khác
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// Task thống kê và maintenance
void statsTask(void *pvParameters) {
  for (;;) {
    uint32_t now = millis();
    
    if (now - lastStatsReport >= STATS_REPORT_INTERVAL) {
      updateStats();
      printSystemInfo();
      cleanupDuplicateTable();
      lastStatsReport = now;
    }
    
    vTaskDelay(pdMS_TO_TICKS(5000)); // Check mỗi 5 giây
  }
}

// Task watchdog
void watchdogTask(void *pvParameters) {
  for (;;) {
    // Reset watchdog timer - Fixed: Added proper watchdog handling
    if (esp_task_wdt_status(NULL) == ESP_OK) {
      esp_task_wdt_reset();
    }
    
    // Kiểm tra queue health
    UBaseType_t queueItems = uxQueueMessagesWaiting(dataQueue);
    if (queueItems > QUEUE_SIZE * 0.8) {
      Serial.printf("⚠️ Queue high usage: %d/%d\n", queueItems, QUEUE_SIZE);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Hàm helper: Rút gọn MAC address
void compressMac(const char* fullMac, char* compressedMac) {
  // Convert "AA:BB:CC:DD:EE:FF" -> "AABBCCDDEEFF"
  int j = 0;
  for (int i = 0; i < 17; i++) {
    if (fullMac[i] != ':') {
      compressedMac[j++] = fullMac[i];
    }
  }
  compressedMac[j] = '\0';
}

// Kiểm tra trùng lặp với thuật toán tối ưu
bool isDuplicate(const struct_message* msg) {
  uint32_t now = millis();
  char compressedMac[13];
  compressMac(msg->mac, compressedMac);
  
  // Tìm entry matching
  for (int i = 0; i < MAX_SENDERS; i++) {
    if (duplicateTable[i].lastSeen > 0 &&
        strcmp(duplicateTable[i].qrcode, msg->qrcode) == 0 &&
        strcmp(duplicateTable[i].mac, compressedMac) == 0) {
      
      if (now - duplicateTable[i].lastSeen < DUPLICATE_TIMEOUT_MS) {
        // Kiểm tra sequence number để chắc chắn
        if (duplicateTable[i].sequence == msg->sequence) {
          return true; // Chắc chắn là duplicate
        }
      }
      
      // Update entry này
      duplicateTable[i].lastSeen = now;
      duplicateTable[i].sequence = msg->sequence;
      return false;
    }
  }
  
  // Tìm slot trống hoặc LRU (Least Recently Used)
  int targetIndex = -1;
  uint32_t oldestTime = now;
  
  for (int i = 0; i < MAX_SENDERS; i++) {
    if (duplicateTable[i].lastSeen == 0) {
      targetIndex = i; // Empty slot
      break;
    }
    
    if (duplicateTable[i].lastSeen < oldestTime) {
      oldestTime = duplicateTable[i].lastSeen;
      targetIndex = i;
    }
  }
  
  // Thêm entry mới
  if (targetIndex != -1) {
    strncpy(duplicateTable[targetIndex].qrcode, msg->qrcode, 23);
    duplicateTable[targetIndex].qrcode[23] = '\0';
    strcpy(duplicateTable[targetIndex].mac, compressedMac);
    duplicateTable[targetIndex].lastSeen = now;
    duplicateTable[targetIndex].sequence = msg->sequence;
  }
  
  return false;
}

// Dọn dẹp bảng duplicate với thống kê
void cleanupDuplicateTable() {
  uint32_t now = millis();
  int cleaned = 0;
  uint16_t activeSenders = 0;
  
  for (int i = 0; i < MAX_SENDERS; i++) {
    if (duplicateTable[i].lastSeen > 0) {
      if (now - duplicateTable[i].lastSeen > DUPLICATE_TIMEOUT_MS * 3) {
        // Xóa entry quá cũ (3x timeout)
        memset(&duplicateTable[i], 0, sizeof(duplicate_entry_t));
        cleaned++;
      } else {
        activeSenders++; // Đếm senders còn active
      }
    }
  }
  
  // Update statistics
  if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    systemStats.activeSenders = activeSenders;
    systemStats.lastCleanup = now;
    xSemaphoreGive(statsMutex);
  }
  
  if (cleaned > 0) {
    Serial.printf("🧹 Cleaned %d old entries | Active senders: %d/%d\n", 
                  cleaned, activeSenders, MAX_SENDERS);
  }
}

// Cập nhật thống kê
void updateStats() {
  if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    systemStats.uptime = millis() / 1000;
    systemStats.queueUsage = (uxQueueMessagesWaiting(dataQueue) * 100) / QUEUE_SIZE;
    xSemaphoreGive(statsMutex);
  }
}

// In thông tin hệ thống
void printSystemInfo() {
  Serial.println("📊 === SYSTEM STATISTICS ===");
  
  if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.printf("⏱️ Uptime: %lu seconds\n", systemStats.uptime);
    Serial.printf("📦 Packets received: %lu\n", systemStats.packetsReceived);
    Serial.printf("❌ Packets dropped: %lu\n", systemStats.packetsDropped);
    Serial.printf("🔄 Duplicates filtered: %lu\n", systemStats.duplicatesFiltered);
    Serial.printf("⚠️ Processing errors: %lu\n", systemStats.processingErrors);
    Serial.printf("📊 Queue usage: %d%%\n", systemStats.queueUsage);
    Serial.printf("📡 Active senders: %d/%d\n", systemStats.activeSenders, MAX_SENDERS);
    xSemaphoreGive(statsMutex);
  }
  
  Serial.printf("🧠 Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("📡 WiFi channel: %d\n", WiFi.channel());
  
  if (SPIFFS.totalBytes() > 0) {
    Serial.printf("💾 SPIFFS: %d/%d bytes used\n", 
                  SPIFFS.usedBytes(), SPIFFS.totalBytes());
  }
  Serial.println("================================\n");
}

// Log dữ liệu vào file
void logToFile(const struct_message* msg) {
  // Kiểm tra kích thước file
  if (SPIFFS.exists(LOG_FILE)) {
    File file = SPIFFS.open(LOG_FILE, "r");
    if (file.size() > MAX_LOG_SIZE) {
      file.close();
      SPIFFS.remove(LOG_FILE);
      Serial.println("📁 Log file rotated");
    } else {
      file.close();
    }
  }
  
  // Ghi log
  File file = SPIFFS.open(LOG_FILE, "a");
  if (file) {
    DynamicJsonDocument doc(200);
    doc["timestamp"] = millis();
    doc["mac"] = msg->mac;
    doc["qrcode"] = msg->qrcode;
    doc["sequence"] = msg->sequence;
    doc["rssi"] = msg->rssi;
    doc["sender_ts"] = msg->timestamp;
    
    serializeJson(doc, file);
    file.println();
    file.close();
  }
}

// Serial command handler
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "stats") {
      updateStats();
      printSystemInfo();
    }
    else if (cmd == "reset") {
      Serial.println("🔄 Resetting statistics...");
      if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        systemStats = {0};
        xSemaphoreGive(statsMutex);
      }
      memset(duplicateTable, 0, sizeof(duplicateTable));
    }
    else if (cmd == "log on") {
      loggingEnabled = true;
      Serial.println("📝 Logging enabled");
    }
    else if (cmd == "log off") {
      loggingEnabled = false;
      Serial.println("📝 Logging disabled");
    }
    else if (cmd == "senders") {
      Serial.printf("📡 Sender Status (Active: %d/%d):\n", systemStats.activeSenders, MAX_SENDERS);
      uint32_t now = millis();
      for (int i = 0; i < MAX_SENDERS && i < 20; i++) { // Chỉ hiển thị 20 đầu
        if (duplicateTable[i].lastSeen > 0) {
          uint32_t ageSec = (now - duplicateTable[i].lastSeen) / 1000;
          Serial.printf("  %s | %s | %lus ago\n", 
                       duplicateTable[i].mac, 
                       duplicateTable[i].qrcode,
                       ageSec);
        }
      }
      if (systemStats.activeSenders > 20) {
        Serial.printf("  ... và %d senders khác\n", systemStats.activeSenders - 20);
      }
    }
    else if (cmd == "help") {
      Serial.println("Available commands:");
      Serial.println("- stats: Show system statistics");
      Serial.println("- reset: Reset all statistics");
      Serial.println("- log on/off: Enable/disable file logging");
      Serial.println("- senders: Show active senders list");
      Serial.println("- help: Show this help");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32 ESP-NOW Receiver v2.0 Starting...");
  
  // Khởi tạo SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("✅ SPIFFS mounted successfully");
  } else {
    Serial.println("❌ SPIFFS mount failed - logging disabled");
    loggingEnabled = false;
  }
  
  // Khởi tạo WiFi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Set channel cố định
  
  Serial.print("📶 MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("✅ ESP-NOW initialized");
  
  // Tạo mutex cho statistics
  statsMutex = xSemaphoreCreateMutex();
  
  // Tạo queue
  dataQueue = xQueueCreate(QUEUE_SIZE, sizeof(struct_message));
  if (dataQueue == NULL) {
    Serial.println("❌ Failed to create queue");
    return;
  }
  Serial.printf("✅ Duplicate table created (size: %d entries, %d bytes)\n", 
                MAX_SENDERS, sizeof(duplicateTable));
  
  // Khởi tạo duplicate table
  memset(duplicateTable, 0, sizeof(duplicateTable));
  
  // Enable watchdog timer (optional - uncomment if needed)
  // esp_task_wdt_init(30, true); // 30 second timeout
  
  // Tạo các tasks
  xTaskCreatePinnedToCore(processTask, "ProcessTask", 8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(statsTask, "StatsTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(watchdogTask, "WatchdogTask", 2048, NULL, 1, NULL, 0);
  
  Serial.println("✅ All tasks created");
  Serial.println("📋 Type 'help' for available commands");
  Serial.println("🎯 System ready - waiting for ESP-NOW packets...\n");
  
  printSystemInfo();
}

void loop() {
  handleSerialCommands();
  vTaskDelay(pdMS_TO_TICKS(100));
}