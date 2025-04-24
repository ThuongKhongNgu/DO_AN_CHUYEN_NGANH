#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

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

// ==== Phần này dành cho TDS ====
const int TDS_Sensor_pin = 35; // sửa lại để không trùng chân
const float VREF = 3.3;                                                                                                         
const int SCOUNT = 10;
int arr[SCOUNT];
int index_arr = 0;
float temp = 28.0;

// ==== Cảm biến analog ====
#define PH_PIN 25
#define LDR 36
#define sensorPower 27
#define sensorPin 34

// ==== Trạng thái ====
bool may_bom_vao_on = false;
bool may_bom_ra_on = false;
bool den_ao_nuoi_on = false;

// ==== Biến lưu hàm millis ====
unsigned long now = millis();

// ==== WiFi & Firebase ====
const char* WIFI_SSID = "Trieu Ninh";
const char* WIFI_PASS = "12344321";
const char* API_KEY = "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg";
const char* DATABASE_URL = "https://nckh-8e369-default-rtdb.firebaseio.com/";

// ==== Firebase Khai báo ====
FirebaseData fbdo_sensor;
FirebaseData fbdo_control;
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

volatile bool buttonFlags[BUTTONS] = {false};
bool status[BUTTONS] = {false};
unsigned long lastDebounceTime[BUTTONS] = {0};
const unsigned long debounceDelay = 50;         // chống dội

// ==== Hàm kiểm tra lỗi ====
void tokenStatusCallback(token_info_t info) {
  if (info.status == token_status_error) {
    Serial.println("Lỗi token: " + String(info.error.message.c_str()));
  }
}

// ==== Hàm connect wifi ====
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nĐã kết nối WiFi!");
}

// ==== Hàm connect fire base ====
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

// ==== Hàm gửi giá trị nút nhấn ====
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

// ==== Hàm đọc giá trị từ fire khi có bất ky thay đổi nào ====
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

          Send_data_button_by_firebase(path.c_str(), status[i]);
          buttonFlags[i] = false;
        }
      } else {
        Serial.println("Lỗi đọc Firebase: " + fbdo_control.errorReason());
      }
    }
  }
}

// ==== Hàm này dùng để lọc nhiễu kết quả của TDS ====
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
  }
  else {
    return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
}

// ==== Hàm đọc TDS ====
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

// ==== Hàm đọc cảm biến nước ====
float water_sensor() {
  digitalWrite(sensorPower, HIGH);
  delay(300);
  int val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return (val / 1450.0) * 100.0;
}

// ==== Hàm đọc pH ====
float readPH() {
  int raw = analogRead(PH_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  return 7 + ((2.50 - voltage) / 0.18);
}

// ==== Hàm đọc cảm biến ánh sáng ====
int LDR_Cal() {
  return analogRead(LDR);
}

// ==== Hàm ĐK máy bơm vào theo điều kiện ====
void controlMayBomVao(float level) {
  if (level < 20.0 && !may_bom_vao_on) {
    Serial.println("MAY_BOM_VAO ON (AUTO)");
    digitalWrite(MAY_BOM_VAO, HIGH);
    digitalWrite(MAY_BOM_RA, LOW);
    may_bom_vao_on = true;
    may_bom_ra_on = false;
    Send_data_button_by_firebase("Control/MAY_BOM_VAO", true);
    Send_data_button_by_firebase("Control/MAY_BOM_RA", false);
  } else if (level >= 30.0 && may_bom_vao_on) {
    Serial.println("MAY_BOM_VAO OFF (AUTO)");
    digitalWrite(MAY_BOM_VAO, LOW);
    may_bom_vao_on = false;
    Send_data_button_by_firebase("Control/MAY_BOM_VAO", false);
  }
}

// ==== Hàm ĐK máy bơm ra theo điều kiện ====
void controlMayBomRa(float level) {
  if (level > 80.0 && !may_bom_ra_on) {
    Serial.println("MAY_BOM_RA ON (AUTO)");
    digitalWrite(MAY_BOM_RA, HIGH);
    digitalWrite(MAY_BOM_VAO, LOW);
    may_bom_ra_on = true;
    may_bom_vao_on = false;
    Send_data_button_by_firebase("Control/MAY_BOM_RA", true);
    Send_data_button_by_firebase("Control/MAY_BOM_VAO", false);
  } else if (level <= 70.0 && may_bom_ra_on) {
    Serial.println("MAY_BOM_RA OFF (AUTO)");
    digitalWrite(MAY_BOM_RA, LOW);
    may_bom_ra_on = false;
    Send_data_button_by_firebase("Control/MAY_BOM_RA", false);
  }
}

// ==== Hàm ĐK LED theo điều kiện ====
void controlDenAoNuoi(int ldr) {
  if (ldr > 2700 && !den_ao_nuoi_on) {
    Serial.println("DEN_AO_NUOI ON (AUTO)");
    digitalWrite(LED, HIGH);
    den_ao_nuoi_on = true;
    Send_data_button_by_firebase("Control/LED", true);
  } else if (ldr <= 2700 && den_ao_nuoi_on) {
    Serial.println("DEN_AO_NUOI OFF (AUTO)");
    digitalWrite(LED, LOW);
    den_ao_nuoi_on = false;
    Send_data_button_by_firebase("Control/LED", false);
  }
}

// Xử lý nút nhấn
void Xu_ly_Button() {
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
}

// Gửi dữ liệu cảm biến mỗi 5s
void Send_Data() {
  static unsigned long lastSensorTime = 0;
  if (now - lastSensorTime > 5000) {
    lastSensorTime = now;
    float pH = readPH();
    int light = LDR_Cal();
    float water = water_sensor();
    float TDS = TDS_Cal();

    if (TDS >= 0) {
      Firebase.RTDB.setString(&fbdo_sensor, "Sensor/pH", String(pH, 2));
      Firebase.RTDB.setInt(&fbdo_sensor, "Sensor/Light", light);
      Firebase.RTDB.setString(&fbdo_sensor, "Sensor/Water", String(water, 2));
      Firebase.RTDB.setFloat(&fbdo_sensor, "Sensor/Tds", TDS);
      Serial.printf("Sensor => pH: %.2f | Light: %d | Water: %.2f%% | Tds: %.2f\n", pH, light, water, TDS);
    }
    controlMayBomVao(water);
    controlMayBomRa(water);
    controlDenAoNuoi(light);
  }
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
  pinMode(TDS_Sensor_pin, INPUT);
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
  now = millis();
  Xu_ly_Button();
  Send_Data();
  Read_Data_By_Firebase();
}