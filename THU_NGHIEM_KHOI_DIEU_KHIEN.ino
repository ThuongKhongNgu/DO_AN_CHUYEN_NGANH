#include <HardwareSerial.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <WiFi.h>
#include "time.h"
#include <sys/time.h>

HardwareSerial zigbeeSerial(1);

// Đám này cho LCD ILI9341
#define TFT_CS 0
#define TFT_DC 2
#define TFT_MOSI 14
#define TFT_SCLK 12
#define TFT_RST 13

//Đám này dành cho RTC ESP32
const char* ssid = "Trieu Ninh";
const char* password = "12344321";
const char* ntpServer = "time.nist.gov";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 3600;

//Đám này cho nút nhấn và LED để điều khiển các ngoại vi sau này
#define FAN_BUTTON 26
#define OXI_BUTTON 25 
#define MSP_BUTTON 5 //5
#define MRTA_BUTTON 15  //15
#define MBV_BUTTON 18
#define MBR_BUTTON 19
#define CHOOSE_MODE 27 
#define CLEAR_DS 33

volatile bool quat = false;
volatile bool oxi = false;
volatile bool msp = false;
volatile bool mrta = false;
volatile bool mbv = false;
volatile bool mbr = false;
volatile bool d_ao_nuoi = false;
volatile bool th_adjust = false;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

enum DisplayUpdateType {UPDATE_TDS, UPDATE_WATER, UPDATE_PH, UPDATE_TIME,
                        UPDATE_FAN, UPDATE_OXI, UPDATE_MSP, UPDATE_MRTA,
                        UPDATE_MBV, UPDATE_MBR, UPDATE_D_AN};

//Struct này ảnh hưởng trực tiếp đến thứ gì được phép cập nhật lên LCD, cũng như thứ tự của chúng
struct DisplayUpdate {
  DisplayUpdateType type;
  float tds_value;
  float water_value;
  float ph_value;
  String time_str;
};

QueueHandle_t displayQueue;

TaskHandle_t received_display_data;
TaskHandle_t fan_button;
TaskHandle_t oxi_button;
TaskHandle_t msp_button;
TaskHandle_t mrta_button;
TaskHandle_t mbv_button;
TaskHandle_t mbr_button;
TaskHandle_t display_ili;
TaskHandle_t send_time;
TaskHandle_t adjust;

int custom_hour = 0;    
int custom_minute = 0;  
int custom_hour_led4 = 0;
int custom_minute_led4 = 0;
int custom_hour_off = 0;
int custom_minute_off = 0;
int custom_hour_off_led4 = 0 ;
int custom_minute_off_led4 = 0;

void setup() {
  Serial.begin(9600);
  zigbeeSerial.begin(115200, SERIAL_8N1, 16, 17);
  pinMode(FAN_BUTTON, INPUT_PULLUP);
  pinMode(OXI_BUTTON, INPUT_PULLUP);
  pinMode(MSP_BUTTON, INPUT_PULLUP);
  pinMode(MRTA_BUTTON, INPUT_PULLUP);
  pinMode(MBV_BUTTON, INPUT_PULLUP);
  pinMode(MBR_BUTTON, INPUT_PULLUP);

  pinMode(CLEAR_DS, INPUT_PULLUP);
  pinMode(CHOOSE_MODE, INPUT_PULLUP);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  //Nếu kết nối không được, in ra một đống dấu chấm
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  
  //Lấy thông tin thời gian, tốt nhất nên có delay 1 tí mắc công sever lỏ
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(5000);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_BLUE, ILI9341_BLACK);
  tft.setTextSize(2);

  tft.setCursor(20, 10);
  tft.println("DO AN CHUYEN NGANH");

  tft.setCursor(20, 30);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.print("TDS_Val:");
  tft.setCursor(20, 55);
  tft.print("Water_Val:");
  tft.setCursor(20, 80);
  tft.print("PH_Val:");

  tft.setCursor(20, 105);
  tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
  tft.print("FAN: ");
  tft.setCursor(70, 105);
  tft.print("OFF");

  tft.setCursor(20, 130);
  tft.print("OXI: ");
  tft.setCursor(70, 130);
  tft.print("OFF");

  tft.setCursor(20, 155);
  tft.print("MRTA: ");
  tft.setCursor(82, 155);
  tft.print("OFF");

  tft.setCursor(20, 180);
  tft.print("M.SP: ");
  tft.setCursor(82, 180);
  tft.print("OFF");
  
  tft.setCursor(20, 205);
  tft.print("M.BV: ");
  tft.setCursor(82, 205);
  tft.print("OFF");

  tft.setCursor(20, 230);
  tft.print("M.BR: ");
  tft.setCursor(82, 230);
  tft.print("OFF");

  tft.setCursor(20, 255);
  tft.print("LED : ");
  tft.setCursor(82, 255);
  tft.print("OFF");

  tft.setCursor(20, 305);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.print("Time: ");

  displayQueue = xQueueCreate(12, sizeof(DisplayUpdate));

  xTaskCreatePinnedToCore(receiveDisplayDataTask, "ReceiveDisplayDataTask", 8192, NULL, 2, &received_display_data, 0);

  xTaskCreatePinnedToCore(fan_buttonf, "fan-button", 4096, NULL, 1, &fan_button, 1);
  xTaskCreatePinnedToCore(oxi_buttonf, "oxi_button", 4096, NULL, 1, &oxi_button, 1);
  xTaskCreatePinnedToCore(msp_buttonf, "msp_button", 4096, NULL, 1, &msp_button, 1);
  xTaskCreatePinnedToCore(mrta_buttonf, "mrta_button", 4096, NULL, 1, &mrta_button, 1);
  xTaskCreatePinnedToCore(mbv_buttonf, "mbv_button", 4096, NULL, 1, &mbv_button, 1);
  xTaskCreatePinnedToCore(mbr_buttonf, "mbr_button", 4096, NULL, 1, &mbr_button, 1);

  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 8192, NULL, 1, &display_ili, 1);
  xTaskCreatePinnedToCore(send_timer, "Send_timer", 8192, NULL, 0, &send_time, 0);
  xTaskCreatePinnedToCore(adjust_time, "Adjust_time", 8192, NULL, 0, &adjust, 1);
}

void displayTask(void *pvParameters) {
  DisplayUpdate receive_update;
  while (true) {
    if (xQueueReceive(displayQueue, &receive_update, portMAX_DELAY)) {
      if (receive_update.type == UPDATE_TDS) {
        tft.fillRect(120, 30, 100, 20, ILI9341_BLACK);
        tft.setCursor(20, 30);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
        tft.print("TDS_Val:");
        tft.print(receive_update.tds_value);
      } 
      else if (receive_update.type == UPDATE_WATER) {
        tft.fillRect(135, 55, 100, 20, ILI9341_BLACK);
        tft.setCursor(20, 55);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
        tft.print("Water_Val:");
        tft.print(receive_update.water_value);
      } 
      else if (receive_update.type == UPDATE_PH) {
        tft.fillRect(120, 80, 100, 20, ILI9341_BLACK);
        tft.setCursor(20, 80);
        tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
        tft.print("PH_Val:");
        tft.print(receive_update.ph_value);
      } 
      else if (receive_update.type == UPDATE_FAN) {
        tft.fillRect(70, 105, 50, 20, ILI9341_BLACK);
        tft.setCursor(70, 105);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(quat ? "ON" : "OFF");
      } 
      else if (receive_update.type == UPDATE_OXI) {
        tft.fillRect(70, 130, 50, 20, ILI9341_BLACK);
        tft.setCursor(70, 130);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(oxi ? "ON" : "OFF");
      }
      else if (receive_update.type == UPDATE_MRTA) {
        tft.fillRect(82, 155, 50, 20, ILI9341_BLACK);
        tft.setCursor(82, 155);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(mrta ? "ON" : "OFF");
      }
      else if (receive_update.type == UPDATE_MSP) {
        tft.fillRect(82, 180, 50, 20, ILI9341_BLACK);
        tft.setCursor(82, 180);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(msp ? "ON" : "OFF");
      }
      else if (receive_update.type == UPDATE_MBV) {
        tft.fillRect(82, 205, 50, 20, ILI9341_BLACK);
        tft.setCursor(82, 205);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(mbv ? "ON" : "OFF");
      }
       else if (receive_update.type == UPDATE_MBR) {
        tft.fillRect(82, 230, 50, 20, ILI9341_BLACK);
        tft.setCursor(82, 230);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(mbr ? "ON" : "OFF");
      }
       else if (receive_update.type == UPDATE_D_AN) {
        tft.fillRect(82, 255, 50, 20, ILI9341_BLACK);
        tft.setCursor(82, 255);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.print(d_ao_nuoi ? "ON" : "OFF");
      }
      else if (receive_update.type == UPDATE_TIME) {
        tft.fillRect(80, 305, 150, 20, ILI9341_BLACK);
        tft.setCursor(80, 305);
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        tft.print(receive_update.time_str);
      }
      vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
  }
}

void ve_lai() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_BLUE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.println("DO AN CHUYEN NGANH");

  tft.setCursor(20, 30);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.print("TDS_Val:");
  tft.setCursor(20, 55);
  tft.print("Water_Val:");
  tft.setCursor(20, 80);
  tft.print("PH_Val:");

  tft.setCursor(20, 105);
  tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
  tft.print("FAN: ");
  tft.setCursor(70, 105);
  tft.print(quat ? "ON" : "OFF");

  tft.setCursor(20, 130);
  tft.print("OXI: ");
  tft.setCursor(70, 130);
  tft.print(oxi ? "ON" : "OFF");

  tft.setCursor(20, 155);
  tft.print("MRTA: ");
  tft.setCursor(82, 155);
  tft.print(mrta ? "ON" : "OFF");

  tft.setCursor(20, 180);
  tft.print("M.SP: ");
  tft.setCursor(82, 180);
  tft.print(msp ? "ON" : "OFF");

  tft.setCursor(20, 205);
  tft.print("M.BV: ");
  tft.setCursor(82, 205);
  tft.print(mbv ? "ON" : "OFF");

  tft.setCursor(20, 230);
  tft.print("M.BR: ");
  tft.setCursor(82, 230);
  tft.print(mbr ? "ON" : "OFF");

  tft.setCursor(20, 255);
  tft.print("LED : ");
  tft.setCursor(82, 255);
  tft.print(d_ao_nuoi ? "ON" : "OFF");

  tft.setCursor(20, 305);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.print("Time: ");
}

void fan_buttonf(void *pvParameters) {
  bool last_button_state = HIGH;
  while (true) {
    bool current_button_state = digitalRead(FAN_BUTTON);
    if (current_button_state == LOW && last_button_state == HIGH) {
      quat = !quat;
      Serial.println(quat ? "FAN_ON" : "FAN_OFF");
      zigbeeSerial.println(quat ? "FAN_ON" : "FAN_OFF");

      DisplayUpdate update = {UPDATE_FAN};
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_button_state = current_button_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void oxi_buttonf(void *pvParameters) {
  bool last_button_state2 = HIGH;
  while (true) {
    bool current_button_state2 = digitalRead(OXI_BUTTON);
    if (current_button_state2 == LOW && last_button_state2 == HIGH) {
      oxi = !oxi;
      Serial.println(oxi ? "OXI_ON" : "OXI_OFF");
      zigbeeSerial.println(oxi ? "OXI_ON" : "OXI_OFF");

      DisplayUpdate update = { UPDATE_OXI };
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_button_state2 = current_button_state2;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void msp_buttonf(void *pvParameters){
  bool last_msp_button_state = HIGH;
  while (true) {
    bool current_msp_button_state = digitalRead(MSP_BUTTON); //Them nut nhan moi o day
    if (current_msp_button_state == LOW && last_msp_button_state == HIGH) {
      msp = !msp;
      Serial.println(msp ? "MSP_ON" : "MSP_OFF");
      zigbeeSerial.println(msp ? "MSP_ON" : "MSP_OFF");

      DisplayUpdate update = {UPDATE_MSP};
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_msp_button_state = current_msp_button_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void mrta_buttonf(void *pvParameters){
  bool last_mrta_button_state = HIGH;
  while (true) {
    bool current_mrta_button_state = digitalRead(MRTA_BUTTON); //Them nut nhan moi o day
    if (current_mrta_button_state == LOW && last_mrta_button_state == HIGH) {
      mrta = !mrta;
      Serial.println(mrta ? "MRTA_ON" : "MRTA_OFF");
      zigbeeSerial.println(mrta ? "MRTA_ON" : "MRTA_OFF");

      DisplayUpdate update = {UPDATE_MRTA};
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_mrta_button_state = current_mrta_button_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void mbv_buttonf(void *pvParameters){
  bool last_mbv_button_state = HIGH;
  while (true) {
    bool current_mbv_button_state = digitalRead(MBV_BUTTON); //Them nut nhan moi o day
    if (current_mbv_button_state == LOW && last_mbv_button_state == HIGH) {
      mbv = !mbv;
      Serial.println(mbv ? "MBV_ON" : "MBV_OFF");
      zigbeeSerial.println(mbv ? "MBV_ON" : "MBV_OFF");

      DisplayUpdate update = {UPDATE_MBV};
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_mbv_button_state = current_mbv_button_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void mbr_buttonf(void *pvParameters){
  bool last_mbr_button_state = HIGH;
  while (true) {
    bool current_mbr_button_state = digitalRead(MBR_BUTTON); //Them nut nhan moi o day
    if (current_mbr_button_state == LOW && last_mbr_button_state == HIGH) {
      mbr = !mbr;
      Serial.println(mbr ? "MBR_ON" : "MBR_OFF");
      zigbeeSerial.println(mbr ? "MBR_ON" : "MBR_OFF");

      DisplayUpdate update = {UPDATE_MBR};
      xQueueSend(displayQueue, &update, portMAX_DELAY);
    }
    last_mbr_button_state = current_mbr_button_state;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void send_timer(void *pvParameters) {
  struct tm timeinfo;
  while (true) {
    if (getLocalTime(&timeinfo, 7000)) {
      int hour = timeinfo.tm_hour;
      int min = timeinfo.tm_min;

      if (hour == custom_hour && min == custom_minute) {
        if (!msp) { 
          msp = true;
          Serial.println("MSP_ON");
          zigbeeSerial.println("MSP_ON");
          DisplayUpdate update = {UPDATE_MSP};
          xQueueSend(displayQueue, &update, portMAX_DELAY);
        }
      }

      if (hour == custom_hour_off && min == custom_minute_off) {
        if (msp) {
          msp = false;
          Serial.println("MSP_OFF");
          zigbeeSerial.println("MSP_OFF");
          DisplayUpdate update = {UPDATE_MSP};
          xQueueSend(displayQueue, &update, portMAX_DELAY);
        }
      }

      if (hour == custom_hour_led4 && min == custom_minute_led4) {
        if (!mrta) { 
          mrta = true;
          Serial.println("MRTA_ON");
          zigbeeSerial.println("MRTA_ON");
          DisplayUpdate update = {UPDATE_MRTA};
          xQueueSend(displayQueue, &update, portMAX_DELAY);
        }
      }

      if (hour == custom_hour_off_led4 && min == custom_minute_off_led4) {
        if (mrta) {
          mrta = false;
          Serial.println("MRTA_OFF");
          zigbeeSerial.println("MRTA_OFF");
          DisplayUpdate update = {UPDATE_MRTA};
          xQueueSend(displayQueue, &update, portMAX_DELAY);
        }
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void adjust_time(void *pvParameters){
  bool last_adjust_state = HIGH;
  bool adjusting = false;
  int mode = 0; 
  while(true){
    bool current_adjust_state = digitalRead(CLEAR_DS);
    if(current_adjust_state == LOW && last_adjust_state == HIGH){
      th_adjust = !th_adjust;
      vTaskSuspend(received_display_data);
      vTaskSuspend(display_ili);
      vTaskSuspend(send_time);
      vTaskSuspend(fan_button);
      vTaskSuspend(oxi_button);
      
      //Gui suspend qua ben kia de ko gui data nua
      // zigbeeSerial.println("SUSPEND_DATA");
      // vTaskDelay(50 / portTICK_PERIOD_MS);
      Serial.println(th_adjust ? "CHINH_TG" : "HET_CHINH_TG");
      
      if (th_adjust) {
        tft.fillScreen(ILI9341_BLACK);
        tft.setCursor(20, 50);
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        tft.setTextSize(3);
        tft.println("MSP BAT");

        tft.setCursor(20, 100);
        tft.print("Gio: ");
        tft.print(get_current_hour(mode));

        tft.setCursor(20, 150);
        tft.print("Phut: ");
        tft.print(get_current_minute(mode));

        adjusting = true;
      } else {
        tft.fillScreen(ILI9341_BLACK);
        ve_lai(); 
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // zigbeeSerial.println("RESUME_DATA");
        // vTaskDelay(50 / portTICK_PERIOD_MS);
        vTaskResume(received_display_data);
        vTaskResume(display_ili);
        vTaskResume(send_time);
        vTaskResume(fan_button);
        vTaskResume(oxi_button);
        adjusting = false;
      }
    }
    if (adjusting) {
      if (digitalRead(CHOOSE_MODE) == LOW) {
        mode = (mode + 1) % 4;
        tft.fillRect(20, 50, 200, 30, ILI9341_BLACK);
        tft.setCursor(20, 50);
        tft.print(get_mode_string(mode));
        vTaskDelay(200 / portTICK_PERIOD_MS);
      }
      
      if (digitalRead(FAN_BUTTON) == LOW) {
        increment_hour(mode);
        tft.fillRect(110, 100, 100, 30, ILI9341_BLACK);
        tft.setCursor(110, 100);
        tft.print(get_current_hour(mode));
        vTaskDelay(200 / portTICK_PERIOD_MS); 
      }

      if (digitalRead(OXI_BUTTON) == LOW) {
        increment_minute(mode);
        tft.fillRect(120, 150, 100, 30, ILI9341_BLACK);
        tft.setCursor(120, 150);
        tft.print(get_current_minute(mode));
        vTaskDelay(200 / portTICK_PERIOD_MS); 
      }
    }
    last_adjust_state = current_adjust_state;
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

int get_current_hour(int mode) {
  switch (mode) {
    case 0: return custom_hour;
    case 1: return custom_hour_led4;
    case 2: return custom_hour_off;
    case 3: return custom_hour_off_led4;
    default: return 0;
  }
}

int get_current_minute(int mode) {
  switch (mode) {
    case 0: return custom_minute;
    case 1: return custom_minute_led4;
    case 2: return custom_minute_off;
    case 3: return custom_minute_off_led4;
    default: return 0;
  }
}

void increment_hour(int mode) {
  switch (mode) {
    case 0: custom_hour = (custom_hour + 1) % 24; break;
    case 1: custom_hour_led4 = (custom_hour_led4 + 1) % 24; break;
    case 2: custom_hour_off = (custom_hour_off + 1) % 24; break;
    case 3: custom_hour_off_led4 = (custom_hour_off_led4 + 1) % 24; break;
  }
}

void increment_minute(int mode) {
  switch (mode) {
    case 0: custom_minute = (custom_minute + 2) % 60; break;
    case 1: custom_minute_led4 = (custom_minute_led4 + 2) % 60; break;
    case 2: custom_minute_off = (custom_minute_off + 2) % 60; break;
    case 3: custom_minute_off_led4 = (custom_minute_off + 2) % 60; break;
  }
}

String get_mode_string(int mode) {
  switch (mode) {
    case 0: return "MSP BAT";
    case 1: return "MRTA BAT";
    case 2: return "MSP TAT";
    case 3: return "MRTA TAT";
    default: return "";
  }
}

void receiveDisplayDataTask(void *pvParameters) {
  while (true) {
    if (zigbeeSerial.available() > 0) {
      String data = zigbeeSerial.readString();
      data.trim();

      if (data.indexOf("TDS_Value:") != -1) {
        String tdsStr = data.substring(data.indexOf("TDS_Value:") + 10);
        tdsStr.trim();
        float tds_value = tdsStr.toFloat();
        DisplayUpdate send_update = {UPDATE_TDS, tds_value};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }
      if (data.indexOf("Water_Level:") != -1) {
        String waterStr = data.substring(data.indexOf("Water_Level:") + 12);
        waterStr.trim();
        float water_value = waterStr.toFloat(); 
        DisplayUpdate send_update = {UPDATE_WATER, 0.0, water_value};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }
      if (data.indexOf("PH_Value:") != -1) {
        String phStr = data.substring(data.indexOf("PH_Value:") + 9);
        phStr.trim();
        float ph_value = phStr.toFloat();
        DisplayUpdate send_update = {UPDATE_PH, 0.0, 0.0, ph_value};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }
      
      if (data.indexOf("MBV_ON") != -1) {
        Serial.println("MAY_BOM_VAO_ON");
        mbv = true;
        DisplayUpdate send_update = {UPDATE_MBV, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      } else if (data.indexOf("MBV_OFF") != -1) {
        Serial.println("MAY_BOM_VAO_OFF");
        mbv = false;
        DisplayUpdate send_update = {UPDATE_MBV, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }

      if (data.indexOf("MBR_ON") != -1) {
        Serial.println("MAY_BOM_RA_ON");
        mbr = true;
        DisplayUpdate send_update = {UPDATE_MBR, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      } else if (data.indexOf("MBR_OFF") != -1) {
        Serial.println("MAY_BOM_RA_OFF");
        mbr = false;
        DisplayUpdate send_update = {UPDATE_MBR, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }

      if (data.indexOf("D_AN_ON") != -1){
        Serial.println("DEN_AO_NUOI_ON");
        d_ao_nuoi = true;
        DisplayUpdate send_update = {UPDATE_D_AN, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }
      else if (data.indexOf("D_AN_OFF") != -1){
        Serial.println("DEN_AO_NUOI_OFF");
        d_ao_nuoi = false;
        DisplayUpdate send_update = {UPDATE_D_AN, 0.0, 0.0, 0.0, ""};
        xQueueSend(displayQueue, &send_update, portMAX_DELAY);
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    //Tạo 1 chuỗi để lưu toàn bộ giá trị thời gian
    char time_str_buff[40];
    //Hàm có sẵn trong thư viện time.h, nó giúp chuyển toàn bộ giá trị RTC thành 1 chuỗi char
    //Cú pháp: size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
    strftime(time_str_buff, sizeof(time_str_buff), "%H:%M  %d-%m", &timeinfo);
    //Tạo 1 chuỗi "giờ hiện tại" và lấy từ chuỗi "time_str_buff" gán vào
    String current_time = String(time_str_buff);
    //Lại tiếp tục dục nó vào hàng đợi cập nhật LCD
    //Lưu ý về 2 số 0.0 
    DisplayUpdate update = {UPDATE_TIME, 0.0, 0.0, 0.0, current_time};
    xQueueSend(displayQueue, &update, portMAX_DELAY);
  } else {
    Serial.println("Khong biet gio giac");
  }
  vTaskDelay(15000 / portTICK_PERIOD_MS);
}