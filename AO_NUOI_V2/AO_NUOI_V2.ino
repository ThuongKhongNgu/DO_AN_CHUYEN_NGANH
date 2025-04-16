#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ==== KHAI BÁO CHÂN ====
const int SW1 = 27, SW2 = 26, SW3 = 25;
const int FAN = 14, OXI = 12, MRTA = 13;
#define TCRT5000_PIN 36

int tcrtValue = 0;
int lastButtonState1 = HIGH, buttonState1 = HIGH, Status1 = 0;
int lastButtonState2 = HIGH, buttonState2 = HIGH, Status2 = 0;
int lastButtonState3 = HIGH, buttonState3 = HIGH, Status3 = 0;

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
    if (Firebase.RTDB.getString(&fbdo, "Control/MRTA")) {
      Status3 = fbdo.stringData().toInt();
    }
  }
}

// === Đọc cảm biến TCRT5000 ===
void readTCRT5000() {
  tcrtValue = analogRead(TCRT5000_PIN);
  Serial.print("Giá trị TCRT5000: ");
  Serial.println(tcrtValue);

  if (Firebase.ready() && signUpOK) {
    if (Firebase.RTDB.setInt(&fbdo, "Sensor/TCRT5000", tcrtValue)) {
      Serial.println(" Gửi giá trị TCRT5000 lên Firebase thành công");
    } else {
      Serial.println(" Gửi giá trị TCRT5000 thất bại: " + String(fbdo.errorReason().c_str()));
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  pinMode(FAN, OUTPUT);
  pinMode(OXI, OUTPUT);
  pinMode(MRTA, OUTPUT);

  digitalWrite(FAN, LOW);
  digitalWrite(OXI, LOW);
  digitalWrite(MRTA, LOW);

  connectWiFi();
  connectFirebase();
}

void loop() {
  unsigned long now = millis();

  // Kiểm tra nút nhấn và cập nhật trạng thái
  ButtonCheck(buttonState1, lastButtonState1, SW1, Status1, "Control/FAN");
  digitalWrite(FAN, Status1);

  ButtonCheck(buttonState2, lastButtonState2, SW2, Status2, "Control/OXI");
  digitalWrite(OXI, Status2);

  ButtonCheck(buttonState3, lastButtonState3, SW3, Status3, "Control/MRTA");
  digitalWrite(MRTA, Status3);

  // Đọc Firebase điều khiển
  if (now - prevReadFirebase >= FirebaseControl) {
    prevReadFirebase = now;
    readFirebaseData();
  }

  // Đọc cảm biến
  if (now - prevReadSensor >= FirebaseSensor) {
    prevReadSensor = now;
    readTCRT5000();
  }
}
