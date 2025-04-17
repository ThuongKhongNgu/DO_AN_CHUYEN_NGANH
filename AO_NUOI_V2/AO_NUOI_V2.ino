#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ==== KHAI BÁO CHÂN ====
const int SW1 = 23, SW2 = 22, SW3 = 21, SW4 = 19, SW5 = 18, SW6 = 5, SW7 = 17;
const int FAN = 16, OXI = 4, MSP = 0, MRTA = 2, MAY_BOM_VAO = 15, MAY_BOM_RA = 32;

#define PH_PIN 25
#define LDR 36

//Phần này dành cho cảm biến mức nước 
#define sensorPower 27
#define sensorPin 34
int val = 0;


// === === === === === === === === === === === === ===
int lastButtonState1 = HIGH, buttonState1 = HIGH, Status1 = 0;
int lastButtonState2 = HIGH, buttonState2 = HIGH, Status2 = 0;
int lastButtonState3 = HIGH, buttonState3 = HIGH, Status3 = 0;
int lastButtonState4 = HIGH, buttonState4 = HIGH, Status4 = 0;
int lastButtonState5 = HIGH, buttonState5 = HIGH, Status5 = 0;
int lastButtonState6 = HIGH, buttonState6 = HIGH, Status6 = 0;
int lastButtonState7 = HIGH, buttonState7 = HIGH, Status7 = 0;

// ==== WiFi & Firebase ====
const char* WIFI_SSID = "Trieu Ninh";
const char* WIFI_PASS = "12344321";
const char* API_KEY = "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg";
const char* DATABASE_URL = "https://nckh-8e369-default-rtdb.firebaseio.com/";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signUpOK = false;

unsigned long prevReadFirebase = 0;
unsigned long prevReadSensor = 0;
const unsigned long FirebaseControl = 1000;
const unsigned long FirebaseSensor = 1500;

void tokenStatusCallback(token_info_t info) {
  if (info.status == token_status_error) {
    Serial.println("Lỗi token: " + String(info.error.message.c_str()));
  }
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n Đã kết nối WiFi!");
}

void connectFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println(" Đăng ký Firebase thành công!");
    signUpOK = true;
  } else {
    Serial.println(" Lỗi đăng ký Firebase: " + String(config.signer.signupError.message.c_str()));
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// === Hàm kiểm tra nút nhấn đơn giản ===
void ButtonCheck(int& buttonState, int& lastState, int pinSW, int& status, const char* path) {
  buttonState = digitalRead(pinSW);
  if (buttonState == LOW && lastState == HIGH) {
    status = !status;
    Serial.printf("%s %s (Từ nút nhấn)\n", path, status ? "BẬT" : "TẮT");

    if (Firebase.ready() && signUpOK) {
      String statusStr = status ? "1" : "0";
      if (Firebase.RTDB.setString(&fbdo, path, statusStr)) {
        Serial.printf(" Gửi %s = %s lên Firebase thành công\n", path, statusStr.c_str());
      } else {
        Serial.println(" Gửi Firebase thất bại: " + String(fbdo.errorReason().c_str()));
      }
    }
  }
  lastState = buttonState;
}


// === Đọc dữ liệu điều khiển từ Firebase ===
void readFirebaseData() {
  if (Firebase.ready() && signUpOK) {
    if (Firebase.RTDB.getString(&fbdo, "Control/FAN")) {
      Status1 = fbdo.stringData().toInt();
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/OXI")) {
      Status2 = fbdo.stringData().toInt();
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/MSP")) {
      Status3 = fbdo.stringData().toInt();
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/MRTA")) {
      Status4 = fbdo.stringData().toInt();
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/MAY_BOM_VAO")) {
      Status5 = fbdo.stringData().toInt();
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/MAY_BOM_RA")) {
      Status6 = fbdo.stringData().toInt();
    }
  }
}

// === Đọc giá trị cảm biến mức nước ===
float water_sensor(){
  float level = readSensor();
  level = (level/1450.0) * 100.0;
  return level;
}

int readSensor() {
    digitalWrite(sensorPower, HIGH);    
    delay(10);                 
    val = analogRead(sensorPin);        
    digitalWrite(sensorPower, LOW);        
    return val;                 
}

// === Đọc giá trị cảm biến pH ===
float readPH() {
    int rawValue = analogRead(PH_PIN);  
    float voltage = (rawValue / 4095.0) * 3.3; 
    return 7 + ((2.50 - voltage) / 0.18);  
}

// === Đọc giá trị cảm biến ánh sáng ===
int LDR_Cal(){
  int ldr;
  return ldr = analogRead(LDR);
}

void setup() {
  Serial.begin(115200);

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(SW4, INPUT_PULLUP);
  pinMode(SW5, INPUT_PULLUP);
  pinMode(SW6, INPUT_PULLUP);
  pinMode(SW7, INPUT_PULLUP);
  pinMode(LDR, INPUT); 
  pinMode(PH_PIN, INPUT);

  pinMode(FAN, OUTPUT);
  pinMode(OXI, OUTPUT);
  pinMode(MSP, OUTPUT);
  pinMode(MRTA, OUTPUT);
  pinMode(MAY_BOM_VAO, OUTPUT);
  pinMode(MAY_BOM_RA, OUTPUT);
  pinMode(sensorPower, OUTPUT);

  digitalWrite(FAN, LOW);
  digitalWrite(OXI, LOW);
  digitalWrite(MSP, LOW);
  digitalWrite(MRTA, LOW);
  digitalWrite(MAY_BOM_VAO, LOW);
  digitalWrite(MAY_BOM_RA, LOW);
  digitalWrite(sensorPower, LOW);

  connectWiFi();
  connectFirebase();
}

void loop() {
  unsigned long now1 = millis();
  unsigned long now2 = millis();

  if(now1 - prevReadFirebase >FirebaseControl){
    // Kiểm tra nút nhấn và cập nhật trạng thái
    ButtonCheck(buttonState1, lastButtonState1, SW1, Status1, "Control/FAN");
    digitalWrite(FAN, Status1);

    ButtonCheck(buttonState2, lastButtonState2, SW2, Status2, "Control/OXI");
    digitalWrite(OXI, Status2);

    ButtonCheck(buttonState3, lastButtonState3, SW3, Status3, "Control/MSP");
    digitalWrite(MSP, Status3);

    ButtonCheck(buttonState4, lastButtonState4, SW4, Status4, "Control/MRTA");
    digitalWrite(MRTA, Status4);

    ButtonCheck(buttonState5, lastButtonState5, SW5, Status5, "Control/MAY_BOM_VAO");
    digitalWrite(MAY_BOM_VAO, Status5);

    ButtonCheck(buttonState6, lastButtonState6, SW6, Status6, "Control/MAY_BOM_RA");
    digitalWrite(MAY_BOM_RA, Status6);

    readFirebaseData();
    prevReadFirebase = now1;
  }
  if(now2 - prevReadSensor >FirebaseSensor){
    float phValue = readPH();
    if (Firebase.ready() && signUpOK) {
      Firebase.RTDB.setFloat(&fbdo, "Sensor/pH", phValue);
      Serial.printf("Đã gửi pH: %.2f\n", phValue);
    }

    int lightValue = LDR_Cal();
    if (Firebase.ready() && signUpOK) {
      Firebase.RTDB.setInt(&fbdo, "Sensor/Light", lightValue);
      Serial.printf("Đã gửi Ánh sáng: %d\n", lightValue);
    }

    float waterLevel = water_sensor();
    if (Firebase.ready() && signUpOK) {
      Serial.println(waterLevel);
      Firebase.RTDB.setFloat(&fbdo, "Sensor/Water_Level", waterLevel);
      Serial.printf("Đã gửi Mức nước: %.2f %%\n", waterLevel);
      }
      prevReadSensor = now2;
    }
  }
