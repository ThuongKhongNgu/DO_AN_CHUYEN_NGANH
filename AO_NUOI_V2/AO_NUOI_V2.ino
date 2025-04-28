#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

// ======== Thông tin WiFi - Firebase =========
#define WIFI_SSID "Trieu Ninh"
#define WIFI_PASSWORD "12344321"
#define API_KEY "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg"
#define DATABASE_URL "https://nckh-8e369-default-rtdb.firebaseio.com/"

// ======== Cấu hình chân kết nối =========
// Nút nhấn
#define SW1 23
#define SW2 22
#define SW3 21
#define SW4 19
#define SW5 18
#define SW6 5
#define SW7 17

// Thiết bị
#define FAN 16         
#define OXI 4          
#define MSP 2          
#define MRTA 15         
#define MAY_BOM_VAO 26  
#define MAY_BOM_RA 32   
#define LED 33          

// Cảm biến
#define PH_PIN 25        // Chân cảm biến pH
#define LDR 36           // Chân cảm biến ánh sáng (LDR)
#define sensorPower 27   // Chân cấp nguồn cho cảm biến nước
#define sensorPin 34     // Chân cảm biến nước
#define TDS_Sensor_pin 35 // Chân cảm biến TDS

#define DEBOUNCE_DELAY 150  // Thời gian chống dội nút (ms)

// ======== Cấu hình múi giờ =========
const char* timezone = "ICT-7"; // ICT: Indochina Time, GMT+7

// ======== Biến toàn cục =========
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Cờ ngắt cho các nút nhấn
volatile bool buttonPressed[7] = {false, false, false, false, false, false, false};
unsigned long lastDebounceTime[7] = {0, 0, 0, 0, 0, 0, 0};

// Trạng thái các thiết bị
bool deviceState[7] = {false, false, false, false, false, false, false}; // FAN, OXI, MSP, MRTA, MAY_BOM_VAO, MAY_BOM_RA, LED

// Biến theo dõi phút cuối cùng đã chạy cho các thiết bị
int lastRunMinuteTaskOn[3] = {-1, -1, -1};  // FAN, OXI, MSP (ON)
int lastRunMinuteTaskOff[3] = {-1, -1, -1}; // FAN, OXI, MSP (OFF)
unsigned long lastNTPUpdate = 0;        // Lần cuối cùng đồng bộ giờ
const unsigned long ntpSyncInterval = 30 * 60 * 1000; // Đồng bộ NTP mỗi 30 phút
unsigned long lastFirebaseScheduleRead = 0; // Thời điểm đọc lịch lần cuối
unsigned long lastSensorRead = 0;           // Thời điểm đọc cảm biến lần cuối
unsigned long lastDebugRead = 0;            // Thời điểm debug tín hiệu nút nhấn
const unsigned long firebaseReadInterval = 10 * 1000; // Đọc mỗi 10 giây
const unsigned long debugInterval = 500;    // Debug mỗi 500ms

// Biến lưu thời gian từ Firebase cho FAN
int task1HourFirebase = 5;    
int task1MinuteFirebase = 10;
int task2HourFirebase = 9;
int task2MinuteFirebase = 10;

// Biến lưu thời gian từ Firebase cho OXI
int task1HourFirebaseOXI = 5;
int task1MinuteFirebaseOXI = 10;
int task2HourFirebaseOXI = 5;
int task2MinuteFirebaseOXI = 10;

// Biến lưu thời gian từ Firebase cho MSP
int task1HourFirebaseMSP = 6;
int task1MinuteFirebaseMSP = 10;
int task2HourFirebaseMSP = 6;
int task2MinuteFirebaseMSP = 50;

// ==== Phần dành cho TDS ====
const float VREF = 3.3;
const int SCOUNT = 10;
int arr[SCOUNT];
int index_arr = 0;
float temp = 28.0;

// ======== Ngắt nút nhấn =========
void IRAM_ATTR handleButtonInterrupt1() { buttonPressed[0] = true; }
void IRAM_ATTR handleButtonInterrupt2() { buttonPressed[1] = true; }
void IRAM_ATTR handleButtonInterrupt3() { buttonPressed[2] = true; }
void IRAM_ATTR handleButtonInterrupt4() { buttonPressed[3] = true; }
void IRAM_ATTR handleButtonInterrupt5() { buttonPressed[4] = true; }
void IRAM_ATTR handleButtonInterrupt6() { buttonPressed[5] = true; }
void IRAM_ATTR handleButtonInterrupt7() { buttonPressed[6] = true; }

// ======== Hàm lọt giá trị nhiễu cho cảm biến TDS =========
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (int i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  for (int j = 0; j < iFilterLen - 1; j++) {
    for (int i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        int bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    return bTab[(iFilterLen - 1) / 2];
  } else {
    return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
}

// ======== Hàm đọc cảm biến TDS =========
float TDS_Cal() {
  if (index_arr < SCOUNT) {
    arr[index_arr++] = analogRead(TDS_Sensor_pin);
    return -1;
  } else {
    int arrTemp[SCOUNT];
    for (int i = 0; i < SCOUNT; i++) arrTemp[i] = arr[i];
    float avrg_voltage = getMedianNum(arrTemp, SCOUNT) * VREF / 4095.0;
    float factor = 1.0 + 0.02 * (temp - 25.0);
    float voltage_comp = avrg_voltage / factor;
    float TDS_Val = (133.42 * voltage_comp * voltage_comp * voltage_comp
                     - 255.86 * voltage_comp * voltage_comp
                     + 857.39 * voltage_comp) * 0.5;
    index_arr = 0;
    return TDS_Val;
  }
}

// ======== Hàm đọc cảm biến H2O =========
float water_sensor() {
  digitalWrite(sensorPower, HIGH);
  delay(300);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return (val / 1450.0) * 100.0;
}

// ======== Hàm đọc cảm biến PH =========
float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  return 7 + ((2.50 - voltage) / 0.18);
}

// ======== Hàm đọc cảm biến AS =========
int LDR_Cal() {
  return analogRead(LDR);
}

// ======== Debug tín hiệu nút nhấn =========
void debugButtonPins() {
  int buttonPins[] = {SW1, SW2, SW3, SW4, SW5, SW6, SW7};
  for (int i = 0; i < 7; i++) {
    int buttonState = digitalRead(buttonPins[i]);
    if (buttonState == LOW) { // Nút được nhấn (LOW do INPUT_PULLUP)
      Serial.printf("SW%d (Pin %d) is pressed\n", i + 1, buttonPins[i]);
    }
  }
}

// ======== Hàm setup() =========
void setup() {
  Serial.begin(115200);

  int devicePins[] = {FAN, OXI, MSP, MRTA, MAY_BOM_VAO, MAY_BOM_RA, LED};
  for (int i = 0; i < 7; i++) {
    pinMode(devicePins[i], OUTPUT);
    digitalWrite(devicePins[i], LOW);
  }

  // Cấu hình chân cho các nút nhấn (INPUT_PULLUP)
  int buttonPins[] = {SW1, SW2, SW3, SW4, SW5, SW6, SW7};
  for (int i = 0; i < 7; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Cấu hình chân cho cảm biến
  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);

  // Cài đặt ngắt cho các nút nhấn
  attachInterrupt(digitalPinToInterrupt(SW1), handleButtonInterrupt1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2), handleButtonInterrupt2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3), handleButtonInterrupt3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4), handleButtonInterrupt4, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW5), handleButtonInterrupt5, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW6), handleButtonInterrupt6, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW7), handleButtonInterrupt7, FALLING);

  initWiFi();      // Kết nối WiFi
  initFirebase();  // Kết nối Firebase
  syncTime();      // Đồng bộ thời gian
  readScheduleFromFirebase(); // Đọc thời gian từ Firebase khi khởi động
}

// ======== Hàm loop() =========
void loop() {
  handleButtons();           // Xử lý nhấn nút cho tất cả thiết bị
  readDeviceStatesFromFirebase(); // Đọc trạng thái thiết bị từ Firebase

  // Đọc lịch từ Firebase định kỳ
  if (millis() - lastFirebaseScheduleRead > firebaseReadInterval) {
    readScheduleFromFirebase();
    lastFirebaseScheduleRead = millis();
  }

  // Đọc và gửi giá trị cảm biến lên Firebase định kỳ
  if (millis() - lastSensorRead > firebaseReadInterval) {
    readAndSendSensors();
    lastSensorRead = millis();
  }

  // Debug tín hiệu nút nhấn
  if (millis() - lastDebugRead > debugInterval) {
    debugButtonPins();
    lastDebugRead = millis();
  }

  handleScheduledTask();     // Kiểm tra lịch bật/tắt cho FAN, OXI, MSP

  // Đồng bộ NTP
  if (millis() - lastNTPUpdate > ntpSyncInterval) {
    syncTime();
  }

  delay(200); // Delay để giảm tải CPU
}

// ===============================================
//             Các Hàm Con 
// ===============================================

// ======== Kết nối WiFi =========
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Wi-Fi");
}

// ======== Kết nối Firebase =========
void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Đăng nhập ẩn danh
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Connecting to Firebase");
  } else {
    Serial.printf("Connected to Firebase, reason: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  readDeviceStatesFromFirebase(); // Đọc trạng thái thiết bị lúc khởi động
}

// ======== Đọc thời gian bật/tắt từ Firebase =========
void readScheduleFromFirebase() {
  // Đọc lịch cho FAN
  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/hour_on")) {
    String hourOnStr = fbdo.stringData();
    task1HourFirebase = hourOnStr.toInt();
    Serial.printf("Firebase: FAN Hour (ON) = %d\n", task1HourFirebase);
  } else {
    Serial.println("Failed to read FAN Hour (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/minute_on")) {
    String minuteOnStr = fbdo.stringData();
    task1MinuteFirebase = minuteOnStr.toInt();
    Serial.printf("Firebase: FAN Minute (ON) = %d\n", task1MinuteFirebase);
  } else {
    Serial.println("Failed to read FAN Minute (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/hour_off")) {
    String hourOffStr = fbdo.stringData();
    task2HourFirebase = hourOffStr.toInt();
    Serial.printf("Firebase: FAN Hour (OFF) = %d\n", task2HourFirebase);
  } else {
    Serial.println("Failed to read FAN Hour (OFF)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/minute_off")) {
    String minuteOffStr = fbdo.stringData();
    task2MinuteFirebase = minuteOffStr.toInt();
    Serial.printf("Firebase: FAN Minute (OFF) = %d\n", task2MinuteFirebase);
  } else {
    Serial.println("Failed to read FAN Minute (OFF)");
  }

  // Đọc lịch cho OXI
  if (Firebase.RTDB.getString(&fbdo, "Daily/OXI/hour_on")) {
    String hourOnStr = fbdo.stringData();
    task1HourFirebaseOXI = hourOnStr.toInt();
    Serial.printf("Firebase: OXI Hour (ON) = %d\n", task1HourFirebaseOXI);
  } else {
    Serial.println("Failed to read OXI Hour (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/OXI/minute_on")) {
    String minuteOnStr = fbdo.stringData();
    task1MinuteFirebaseOXI = minuteOnStr.toInt();
    Serial.printf("Firebase: OXI Minute (ON) = %d\n", task1MinuteFirebaseOXI);
  } else {
    Serial.println("Failed to read OXI Minute (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/OXI/hour_off")) {
    String hourOffStr = fbdo.stringData();
    task2HourFirebaseOXI = hourOffStr.toInt();
    Serial.printf("Firebase: OXI Hour (OFF) = %d\n", task2HourFirebaseOXI);
  } else {
    Serial.println("Failed to read OXI Hour (OFF)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/OXI/minute_off")) {
    String minuteOffStr = fbdo.stringData();
    task2MinuteFirebaseOXI = minuteOffStr.toInt();
    Serial.printf("Firebase: OXI Minute (OFF) = %d\n", task2MinuteFirebaseOXI);
  } else {
    Serial.println("Failed to read OXI Minute (OFF)");
  }

  // Đọc lịch cho MSP
  if (Firebase.RTDB.getString(&fbdo, "Daily/MSP/hour_on")) {
    String hourOnStr = fbdo.stringData();
    task1HourFirebaseMSP = hourOnStr.toInt();
    Serial.printf("Firebase: MSP Hour (ON) = %d\n", task1HourFirebaseMSP);
  } else {
    Serial.println("Failed to read MSP Hour (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/MSP/minute_on")) {
    String minuteOnStr = fbdo.stringData();
    task1MinuteFirebaseMSP = minuteOnStr.toInt();
    Serial.printf("Firebase: MSP Minute (ON) = %d\n", task1MinuteFirebaseMSP);
  } else {
    Serial.println("Failed to read MSP Minute (ON)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/MSP/hour_off")) {
    String hourOffStr = fbdo.stringData();
    task2HourFirebaseMSP = hourOffStr.toInt();
    Serial.printf("Firebase: MSP Hour (OFF) = %d\n", task2HourFirebaseMSP);
  } else {
    Serial.println("Failed to read MSP Hour (OFF)");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/MSP/minute_off")) {
    String minuteOffStr = fbdo.stringData();
    task2MinuteFirebaseMSP = minuteOffStr.toInt();
    Serial.printf("Firebase: MSP Minute (OFF) = %d\n", task2MinuteFirebaseMSP);
  } else {
    Serial.println("Failed to read MSP Minute (OFF)");
  }
}

// ======== Đọc và gửi giá trị cảm biến lên Firebase =========
void readAndSendSensors() {
  // Đọc giá trị cảm biến
  float phValue = readPH();
  float tdsValue = TDS_Cal();
  float waterLevel = water_sensor();
  int lightValue = LDR_Cal();

  // Gửi giá trị lên Firebase với 2 số sau dấu phẩy
  if (phValue >= 0) {
    String phStr = String(phValue, 2);
    if (Firebase.RTDB.setString(&fbdo, "Sensor/PH", phStr)) {
      Serial.printf("PH Value: %.2f\n", phValue);
    } else {
      Serial.println("Failed to send PH value");
    }
  }

  if (tdsValue >= 0) {
    String tdsStr = String(tdsValue, 2);
    if (Firebase.RTDB.setString(&fbdo, "Sensor/TDS", tdsStr)) {
      Serial.printf("TDS Value: %.2f\n", tdsValue);
    } else {
      Serial.println("Failed to send TDS value");
    }
  }

  if (waterLevel >= 0) {
    String waterStr = String(waterLevel, 2);
    if (Firebase.RTDB.setString(&fbdo, "Sensor/Water", waterStr)) {
      Serial.printf("Water Level: %.2f%%\n", waterLevel);
    } else {
      Serial.println("Failed to send Water Level");
    }
  }

  if (lightValue >= 0) {
    if (Firebase.RTDB.setInt(&fbdo, "Sensor/Light", lightValue)) {
      Serial.printf("Light Value: %d\n", lightValue);
    } else {
      Serial.println("Failed to send Light value");
    }
  }
}

// ======== Xử lý nhấn nút =========
void handleButtons() {
  int devicePins[] = {FAN, OXI, MSP, MRTA, MAY_BOM_VAO, MAY_BOM_RA, LED};
  String devicePaths[] = {"Control/FAN", "Control/OXI", "Control/MSP", "Control/MRTA", "Control/MAY_BOM_VAO", "Control/MAY_BOM_RA", "Control/LED"};

  for (int i = 0; i < 7; i++) {
    if (buttonPressed[i]) {
      buttonPressed[i] = false; // Xử lý xong thì xóa cờ

      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime[i]) > DEBOUNCE_DELAY) { // Chống dội nút
        lastDebounceTime[i] = currentTime;

        deviceState[i] = !deviceState[i]; // Đảo trạng thái thiết bị
        digitalWrite(devicePins[i], deviceState[i] ? HIGH : LOW);

        // Ghi trạng thái mới lên Firebase
        String value = deviceState[i] ? "1" : "0";
        if (Firebase.RTDB.setString(&fbdo, devicePaths[i], value)) {
          Serial.printf("Button %d Pressed -> Updated %s to %s\n", i + 1, devicePaths[i].c_str(), value.c_str());
        } else {
          Serial.println("Button Pressed -> Failed");
        }
      }
    }
  }
}

// ======== Đọc trạng thái thiết bị từ Firebase =========
void readDeviceStatesFromFirebase() {
  int devicePins[] = {FAN, OXI, MSP, MRTA, MAY_BOM_VAO, MAY_BOM_RA, LED};
  String devicePaths[] = {"Control/FAN", "Control/OXI", "Control/MSP", "Control/MRTA", "Control/MAY_BOM_VAO", "Control/MAY_BOM_RA", "Control/LED"};

  for (int i = 0; i < 7; i++) {
    if (Firebase.RTDB.getString(&fbdo, devicePaths[i])) {
      String remoteState = fbdo.stringData();
      bool remoteDeviceState = (remoteState == "1");

      if (remoteDeviceState != deviceState[i]) {
        deviceState[i] = remoteDeviceState;
        digitalWrite(devicePins[i], deviceState[i] ? HIGH : LOW);
        Serial.printf("Firebase Updated -> %s set to %s\n", devicePaths[i].c_str(), remoteState.c_str());
      }
    } else {
      Serial.printf("Failed to read %s state\n", devicePaths[i].c_str());
    }
  }
}

// ======== Xử lý bật/tắt thiết bị theo lịch =========
void handleScheduledTask() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  Serial.printf("Current time: %02d:%02d:%02d, Date: %04d-%02d-%02d\n",
              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
              timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

  // Task 1: Bật FAN theo giờ từ Firebase
  if (timeinfo.tm_hour == task1HourFirebase && timeinfo.tm_min == task1MinuteFirebase && lastRunMinuteTaskOn[0] != timeinfo.tm_min) {
    deviceState[0] = true; // FAN là thiết bị 0
    digitalWrite(FAN, HIGH);
    Firebase.RTDB.setString(&fbdo, "Control/FAN", "1");
    Serial.println("######### Bật FAN tự động theo lịch Firebase #########");
    lastRunMinuteTaskOn[0] = timeinfo.tm_min;
  }

  // Task 2: Tắt FAN theo giờ từ Firebase
  if (timeinfo.tm_hour == task2HourFirebase && timeinfo.tm_min == task2MinuteFirebase && lastRunMinuteTaskOff[0] != timeinfo.tm_min) {
    deviceState[0] = false;
    digitalWrite(FAN, LOW);
    Firebase.RTDB.setString(&fbdo, "Control/FAN", "0");
    Serial.println("######### Tắt FAN tự động theo lịch Firebase #########");
    lastRunMinuteTaskOff[0] = timeinfo.tm_min;
  }

  // Task 3: Bật OXI theo giờ từ Firebase
  if (timeinfo.tm_hour == task1HourFirebaseOXI && timeinfo.tm_min == task1MinuteFirebaseOXI && lastRunMinuteTaskOn[1] != timeinfo.tm_min) {
    deviceState[1] = true; // OXI là thiết bị 1
    digitalWrite(OXI, HIGH);
    Firebase.RTDB.setString(&fbdo, "Control/OXI", "1");
    Serial.println("######### Bật OXI tự động theo lịch Firebase #########");
    lastRunMinuteTaskOn[1] = timeinfo.tm_min;
  }

  // Task 4: Tắt OXI theo giờ từ Firebase
  if (timeinfo.tm_hour == task2HourFirebaseOXI && timeinfo.tm_min == task2MinuteFirebaseOXI && lastRunMinuteTaskOff[1] != timeinfo.tm_min) {
    deviceState[1] = false;
    digitalWrite(OXI, LOW);
    Firebase.RTDB.setString(&fbdo, "Control/OXI", "0");
    Serial.println("######### Tắt OXI tự động theo lịch Firebase #########");
    lastRunMinuteTaskOff[1] = timeinfo.tm_min;
  }

  // Task 5: Bật MSP theo giờ từ Firebase
  if (timeinfo.tm_hour == task1HourFirebaseMSP && timeinfo.tm_min == task1MinuteFirebaseMSP && lastRunMinuteTaskOn[2] != timeinfo.tm_min) {
    deviceState[2] = true; // MSP là thiết bị 2
    digitalWrite(MSP, HIGH);
    Firebase.RTDB.setString(&fbdo, "Control/MSP", "1");
    Serial.println("######### Bật MSP tự động theo lịch Firebase #########");
    lastRunMinuteTaskOn[2] = timeinfo.tm_min;
  }

  // Task 6: Tắt MSP theo giờ từ Firebase
  if (timeinfo.tm_hour == task2HourFirebaseMSP && timeinfo.tm_min == task2MinuteFirebaseMSP && lastRunMinuteTaskOff[2] != timeinfo.tm_min) {
    deviceState[2] = false;
    digitalWrite(MSP, LOW);
    Firebase.RTDB.setString(&fbdo, "Control/MSP", "0");
    Serial.println("######### Tắt MSP tự động theo lịch Firebase #########");
    lastRunMinuteTaskOff[2] = timeinfo.tm_min;
  }
}

// ======== Đồng bộ thời gian từ NTP server =========
void syncTime() {
  Serial.print("Synchronizing time with NTP server...");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7

  time_t now = time(nullptr);
  while (now < 24 * 3600) { // Chờ đến khi có thời gian thực
    delay(100);
    now = time(nullptr);
  }
  Serial.println(" Time synchronized!");

  // Cập nhật lại múi giờ
  setenv("TZ", timezone, 1);
  tzset();

  lastNTPUpdate = millis(); // Ghi nhận lần sync
}