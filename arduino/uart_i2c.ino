#include "M5UnitQRCode.h"
#include <Wire.h>


M5UnitQRCodeI2C qrcodeI2C;   
M5UnitQRCodeUART qrcodeUART; 

#define SDA_PIN 21
#define SCL_PIN 22

TaskHandle_t taskI2CHandle;
TaskHandle_t taskUARTHandle;

bool i2cOK = false;
bool uartOK = false;

// --- Task đọc I2C ---
void taskI2C(void *pvParameters) {
  while (true) {
    if (qrcodeI2C.getDecodeReadyStatus() == 1) {
      uint8_t buffer[512] = {0};
      uint16_t length = qrcodeI2C.getDecodeLength();
      qrcodeI2C.getDecodeData(buffer, length);

      Serial.printf("[I2C] Length: %d, Data: ", length);
      for (int i = 0; i < length; i++) Serial.printf("%c", buffer[i]);
      Serial.println();
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// --- Task đọc UART ---
void taskUART(void *pvParameters) {
  while (true) {
    if (qrcodeUART.available()) {
      String data = qrcodeUART.getDecodeData();
      uint16_t length = data.length();
      Serial.printf("[UART] Length: %d, Data: %s\n", length, data.c_str());
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // --- Init I2C  ---
  Serial.println("Init QRCode I2C...");
  if (qrcodeI2C.begin(&Wire, UNIT_QRCODE_ADDR, SDA_PIN, SCL_PIN, 100000U)) {
    Serial.println("Unit QRCode I2C Init Success");
    qrcodeI2C.setTriggerMode(AUTO_SCAN_MODE);
    i2cOK = true;
  } else {
    Serial.println("Unit QRCode I2C Init Fail ");
  }

  // --- Init UART  ---
  Serial.println("Init QRCode UART...");
  if (qrcodeUART.begin(&Serial2, UNIT_QRCODE_UART_BAUD, 16, 17)) {
    Serial.println("Unit QRCode UART Init Success");
    qrcodeUART.setTriggerMode(AUTO_SCAN_MODE);
    uartOK = true;
  } else {
    Serial.println("Unit QRCode UART Init Fail ");
  }

  // --- Tạo task tương ứng ---
  if (i2cOK) {
    xTaskCreatePinnedToCore(taskI2C, "TaskI2C", 4096, NULL, 1, &taskI2CHandle, 0);
  }

  if (uartOK) {
    xTaskCreatePinnedToCore(taskUART, "TaskUART", 4096, NULL, 1, &taskUARTHandle, 1);
  }

  if (!i2cOK && !uartOK) {
    Serial.println("❌ Không tìm thấy thiết bị QRCode nào (I2C hoặc UART).");
  }
}

void loop() {
  // loop rỗng, FreeRTOS quản lý hết
}
