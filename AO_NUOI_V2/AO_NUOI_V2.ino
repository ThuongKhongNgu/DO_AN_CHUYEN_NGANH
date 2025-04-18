#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ==== Khai báo các chân nút nhấn ====
#define SW1 23
#define SW2 22
#define SW3 21
#define SW4 19
#define SW5 18
#define SW6 5
#define SW7 17

// ==== Khai báo các chân thiết bị ====
#define FAN 16
#define OXI 4
#define MSP 0
#define MRTA 2
#define MAY_BOM_VAO 15
#define MAY_BOM_RA 32
#define LED 33

// ==== Cảm biến analog ====
#define PH_PIN 25
#define LDR 36
#define sensorPower 27
#define sensorPin 34

// ==== WiFi & Firebase ====
const char* WIFI_SSID = "Trieu Ninh";
const char* WIFI_PASS = "12344321";
const char* API_KEY = "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg";
const char* DATABASE_URL = "https://nckh-8e369-default-rtdb.firebaseio.com/";

// ==== Firebase Khai báo ====
FirebaseData fbdo_sensor;   // Dùng cho cảm biến
FirebaseData fbdo_control;  // Dùng cho điều khiển

FirebaseAuth auth;
FirebaseConfig config;

bool signUpOK = false;

// ==== Nút & thiết bị ====
const int BUTTONS = 7;
const int buttonPins[BUTTONS] = {SW1, SW2, SW3, SW4, SW5, SW6, SW7};
const int Control[BUTTONS] = {FAN, OXI, MSP, MRTA, MAY_BOM_VAO, MAY_BOM_RA, LED};
const char* firebasePaths[BUTTONS] = {
  "Control/FAN", "Control/OXI", "Control/MSP",
  "Control/MRTA", "Control/MAY_BOM_VAO", "Control/MAY_BOM_RA", "Control/LED"
};

// ==== Trạng thái ====
volatile bool buttonFlags[BUTTONS] = {false};
bool status[BUTTONS] = {false};
unsigned long lastDebounceTime[BUTTONS] = {0};
const unsigned long debounceDelay = 50;

// ==== Token Callback ====
void tokenStatusCallback(token_info_t info) {
  if (info.status == token_status_error) {
    Serial.println("Lỗi token: " + String(info.error.message.c_str()));
  }
}

// ==== Kết nối WiFi ====
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nĐã kết nối WiFi!");
}

// ==== Kết nối Firebase ====
void connectFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase đăng ký thành công!");
    signUpOK = true;
  } else {
    Serial.println("Firebase lỗi: " + String(config.signer.signupError.message.c_str()));
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// ==== Gửi trạng thái thiết bị ====
void Send_data_button_by_firebase(const char* path, bool state) {
  if (Firebase.ready() && signUpOK) {
    String value = state ? "1" : "0";
    if (Firebase.RTDB.setString(&fbdo_control, path, value)) {
      Serial.println("Đã gửi: " + String(path) + " = " + value);
    } else {
      Serial.println("Gửi thất bại: " + fbdo_control.errorReason());
    }
  }
}

// ==== Nhận dữ liệu từ Firebase ====
void Read_Data_By_Firebase() {
  if (Firebase.ready() && signUpOK) {
    for (int i = 0; i < BUTTONS; i++) {
      String path = firebasePaths[i];
      if (Firebase.RTDB.getString(&fbdo_control, path)) {
        String value = fbdo_control.stringData();
        bool newState = (value == "1");

        if (status[i] != newState) {
          status[i] = newState;
          digitalWrite(Control[i], status[i]);
          Serial.printf("Firebase điều khiển => Thiết bị %d: %s\n", i + 1, status[i] ? "ON" : "OFF");

          Send_data_button_by_firebase(path.c_str(), status[i]); // Cập nhật lại Firebase để đồng bộ
          buttonFlags[i] = false; // Reset flag tránh trùng lặp
        }
      } else {
        Serial.println("Lỗi đọc Firebase: " + fbdo_control.errorReason());
      }
    }
  }
}

// ==== Đọc cảm biến nước ====
float water_sensor() {
  digitalWrite(sensorPower, HIGH);
  delay(1000);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return (val / 1450.0) * 100.0;
}

// ==== Đọc cảm biến pH ====
float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  return 7 + ((2.50 - voltage) / 0.18);
}

// ==== Đọc ánh sáng ====
int LDR_Cal() {
  return analogRead(LDR);
}

// ==== Ngắt nút nhấn ====
void IRAM_ATTR handleButton0() { buttonFlags[0] = true; }
void IRAM_ATTR handleButton1() { buttonFlags[1] = true; }
void IRAM_ATTR handleButton2() { buttonFlags[2] = true; }
void IRAM_ATTR handleButton3() { buttonFlags[3] = true; }
void IRAM_ATTR handleButton4() { buttonFlags[4] = true; }
void IRAM_ATTR handleButton5() { buttonFlags[5] = true; }
void IRAM_ATTR handleButton6() { buttonFlags[6] = true; }

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  connectWiFi();
  connectFirebase();

  for (int i = 0; i < BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(Control[i], OUTPUT);
    digitalWrite(Control[i], LOW);
  }

  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);

  attachInterrupt(digitalPinToInterrupt(SW1), handleButton0, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW2), handleButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW3), handleButton2, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW4), handleButton3, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW5), handleButton4, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW6), handleButton5, FALLING);
  attachInterrupt(digitalPinToInterrupt(SW7), handleButton6, FALLING);
}

// ==== LOOP ====
void loop() {
  unsigned long now = millis();

  // Xử lý nút nhấn
  for (int i = 0; i < BUTTONS; i++) {
    if (buttonFlags[i] && (now - lastDebounceTime[i] > debounceDelay)) {
      lastDebounceTime[i] = now;
      buttonFlags[i] = false;
      status[i] = !status[i];
      digitalWrite(Control[i], status[i]);
      Send_data_button_by_firebase(firebasePaths[i], status[i]);
      Serial.printf("Nút %d nhấn => Thiết bị: %s\n", i + 1, status[i] ? "ON" : "OFF");
    }
  }

  // Gửi cảm biến mỗi 5s
  static unsigned long lastSensorTime = 0;
  if (now - lastSensorTime > 5000) {
    lastSensorTime = now;
    float pH = readPH();
    int light = LDR_Cal();
    float water = water_sensor();

    Firebase.RTDB.setFloat(&fbdo_sensor, "Sensor/pH", pH);
    Firebase.RTDB.setInt(&fbdo_sensor, "Sensor/Light", light);
    Firebase.RTDB.setFloat(&fbdo_sensor, "Sensor/Water", water);

    Serial.printf("Sensor => pH: %.2f | Light: %d | Water: %.2f%%\n", pH, light, water);
  }

  // Nhận trạng thái từ Firebase
  Read_Data_By_Firebase();
}
