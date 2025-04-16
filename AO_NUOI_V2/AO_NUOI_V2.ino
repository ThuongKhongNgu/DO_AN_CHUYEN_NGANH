#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// ======== KHAI BÁO CHÂN =========
const int SW1 = 27;  // Nút cho FAN
const int SW2 = 26;  // Nút cho OXI
const int SW3 = 25;  // Nút cho MRTA

const int FAN = 14;  
const int OXI = 12;   
const int MRTA = 13; 

#define TCRT5000_PIN 36
int tcrtValue = 0;        // Biến lưu giá trị đọc từ cảm biến

// ======== BIẾN TRẠNG THÁI =========
int lastButtonState1 = HIGH, buttonState1 = HIGH, Status1 = 0;
int lastButtonState2 = HIGH, buttonState2 = HIGH, Status2 = 0;
int lastButtonState3 = HIGH, buttonState3 = HIGH, Status3 = 0;

// ======== KẾT NỐI WiFi =========
const char* WIFI_SSID = "Trieu Ninh";
const char* WIFI_PASS = "12344321";

// ======== KẾT NỐI Firebase =========
const char* API_KEY = "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg";
const char* DATABASE_URL = "https://nckh-8e369-default-rtdb.firebaseio.com/";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signUpOK = false;

void tokenStatusCallback(token_info_t info);

// ======== HÀM KẾT NỐI WiFi =========
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nĐã kết nối WiFi!");
}

// ======== HÀM KẾT NỐI Firebase =========
void connectFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Đăng ký Firebase thành công!");
    signUpOK = true;
  } else {
    Serial.println("Lỗi đăng ký Firebase: " + String(config.signer.signupError.message.c_str()));
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void tokenStatusCallback(token_info_t info) {
  if (info.status == token_status_error) {
    Serial.println("Lỗi token: " + String(info.error.message.c_str()));
  }
}

// ======== HÀM ĐỌC Firebase =========
void readFirebaseData() {
  if (Firebase.ready() && signUpOK) {
    if (Firebase.RTDB.getString(&fbdo, "Control/FAN")) {
      Status1 = fbdo.stringData().toInt();
      digitalWrite(FAN, Status1);
      Serial.println(Status1 ? "FAN BẬT (Firebase)" : "FAN TẮT (Firebase)");
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/OXI")) {
      Status2 = fbdo.stringData().toInt();
      digitalWrite(OXI, Status2);
      Serial.println(Status2 ? "OXI BẬT (Firebase)" : "OXI TẮT (Firebase)");
    }
    if (Firebase.RTDB.getString(&fbdo, "Control/MRTA")) {
      Status3 = fbdo.stringData().toInt();
      digitalWrite(MRTA, Status3);
      Serial.println(Status3 ? "MRTA BẬT (Firebase)" : "MRTA TẮT (Firebase)");
    }
  }
}

// ======== HÀM ĐỌC NÚT NHẤN & GỬI FIREBASE =========
void ButtonCheck(int& buttonState, int& lastState, int pinSW, int& status, int pinRelay, const char* path) {
  buttonState = digitalRead(pinSW);
  if (buttonState == LOW && lastState == HIGH) {
    status = !status;
    digitalWrite(pinRelay, status);
    Serial.printf("%s %s (Từ nút nhấn)\n", path, status ? "BẬT" : "TẮT");

    if (Firebase.ready() && signUpOK) {
      String statusStr = status ? "1" : "0";
      if (Firebase.RTDB.setString(&fbdo, path, statusStr)) {
        Serial.printf("Gửi %s = %s lên Firebase thành công\n", path, statusStr.c_str());
      } else {
        Serial.println("Gửi Firebase thất bại: " + String(fbdo.errorReason().c_str()));
      }
    }
  }
}

void readTCRT5000() {
   tcrtValue = analogRead(TCRT5000_PIN);   // Đọc giá trị analog từ cảm biến
   Serial.print("Giá trị đọc được của TCRT5000: ");
   Serial.println(tcrtValue);
 
   // Gửi giá trị cảm biến lên Firebase
   if (Firebase.ready() && signUpOK) {
     if (Firebase.RTDB.setInt(&fbdo, "Sensor/TCRT5000", tcrtValue)) {  // Gửi kiểu int
       Serial.println("Gửi giá trị TCRT5000 lên Firebase thành công");
     } else {
       Serial.println("Gửi giá trị TCRT5000 lên Firebase thất bại: " + String(fbdo.errorReason().c_str()));
     }
   }
 }
// ======== SETUP =========
void setup() {
  Serial.begin(115200);

  // Cấu hình chân
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

// ======== LOOP =========
void loop() {
  readFirebaseData();  // Đọc Firebase

  ButtonCheck(buttonState1, lastButtonState1, SW1, Status1, FAN, "Control/FAN");
  ButtonCheck(buttonState2, lastButtonState2, SW2, Status2, OXI, "Control/OXI");
  ButtonCheck(buttonState3, lastButtonState3, SW3, Status3, MRTA, "Control/MRTA");

  lastButtonState1 = buttonState1;
  lastButtonState2 = buttonState2;
  lastButtonState3 = buttonState3;

  delay(500);  // Giảm spam Firebase
  readTCRT5000();      // Đọc cảm biến TCRT5000 và upload lên Firebase
   delay(1000);          // Delay 500ms để đỡ spam Firebase
}
