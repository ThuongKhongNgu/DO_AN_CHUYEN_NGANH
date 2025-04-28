#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

// ======== Thông tin WiFi =========
#define WIFI_SSID "Trieu Ninh"
#define WIFI_PASSWORD "12344321"

// ======== Thông tin Firebase =========
#define API_KEY "AIzaSyCicNauI0OCVjFMnEpFBqm0OjfhL8TcUNg"
#define DATABASE_URL "https://nckh-8e369-default-rtdb.firebaseio.com/"

// ======== Cấu hình chân kết nối =========
#define FAN_PIN     16      // Quạt nối chân D16
#define BUTTON_PIN  23      // Nút nhấn nối chân D23

#define DEBOUNCE_DELAY 150  // Thời gian chống dội nút (ms)

// ======== Cấu hình múi giờ =========
const char* timezone = "ICT-7"; // ICT: Indochina Time, GMT+7

// ======== Biến toàn cục =========
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

volatile bool buttonPressed = false;  // Cờ báo nút nhấn
unsigned long lastDebounceTime = 0;    // Thời điểm xử lý lần cuối nút nhấn

bool fanState = false;                 // Trạng thái quạt: false = OFF, true = ON
int lastRunMinuteTask1 = -1;           // Phút cuối cùng đã bật quạt tự động
int lastRunMinuteTask2 = -1;           // Phút cuối cùng đã tắt quạt tự động

unsigned long lastNTPUpdate = 0;        // Lần cuối cùng đồng bộ giờ
const unsigned long ntpSyncInterval = 30 * 60 * 1000; // Đồng bộ NTP mỗi 30 phút

unsigned long lastFirebaseScheduleRead = 0; // Thời điểm đọc lịch lần cuối
const unsigned long firebaseReadInterval = 10 * 1000; // Đọc mỗi 10 giây

// Biến lưu thời gian từ Firebase
int task1HourFirebase = 15;    // Giá trị mặc định ban đầu
int task1MinuteFirebase = 15;
int task2HourFirebase = 15;
int task2MinuteFirebase = 16;

// ======== Ngắt nút nhấn =========
void IRAM_ATTR handleButtonInterrupt() {
  buttonPressed = true; // Khi nhấn nút thì set cờ
}

// ======== Hàm setup() =========
void setup() {
  Serial.begin(115200);

  // Cấu hình chân
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // Mặc định tắt quạt

  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING); // Cài ngắt nút nhấn

  initWiFi();      // Kết nối WiFi
  initFirebase();  // Kết nối Firebase
  syncTime();      // Đồng bộ thời gian
  readScheduleFromFirebase(); // Đọc thời gian từ Firebase khi khởi động
}

// ======== Hàm loop() =========
void loop() {
  handleButton();           // Xử lý nhấn nút
  readFanStateFromFirebase(); // Đọc trạng thái quạt từ Firebase

  // Đọc lịch từ Firebase định kỳ
  if (millis() - lastFirebaseScheduleRead > firebaseReadInterval) {
    readScheduleFromFirebase();
    lastFirebaseScheduleRead = millis();
  }

  handleScheduledTask();     // Kiểm tra lịch bật/tắt quạt

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
    Serial.println("Firebase anonymous sign-up successful");
  } else {
    Serial.printf("Firebase anonymous sign-up failed, reason: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  readFanStateFromFirebase(); // Đọc trạng thái quạt lúc khởi động
}

// ======== Đọc thời gian bật/tắt từ Firebase =========
void readScheduleFromFirebase() {
  // Đọc giờ và phút bật quạt (Task 1)
  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/hour_on")) {
    String hourOnStr = fbdo.stringData();
    task1HourFirebase = hourOnStr.toInt(); // Chuyển chuỗi thành số nguyên
    Serial.printf("Firebase: Task1 Hour (ON) = %d\n", task1HourFirebase);
  } else {
    Serial.println("Failed to read Task1 Hour (ON) from Firebase");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/minute_on")) {
    String minuteOnStr = fbdo.stringData();
    task1MinuteFirebase = minuteOnStr.toInt(); // Chuyển chuỗi thành số nguyên
    Serial.printf("Firebase: Task1 Minute (ON) = %d\n", task1MinuteFirebase);
  } else {
    Serial.println("Failed to read Task1 Minute (ON) from Firebase");
  }

  // Đọc giờ và phút tắt quạt (Task 2)
  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/hour_off")) {
    String hourOffStr = fbdo.stringData();
    task2HourFirebase = hourOffStr.toInt(); // Chuyển chuỗi thành số nguyên
    Serial.printf("Firebase: Task2 Hour (OFF) = %d\n", task2HourFirebase);
  } else {
    Serial.println("Failed to read Task2 Hour (OFF) from Firebase");
  }

  if (Firebase.RTDB.getString(&fbdo, "Daily/FAN/minute_off")) {
    String minuteOffStr = fbdo.stringData();
    task2MinuteFirebase = minuteOffStr.toInt(); // Chuyển chuỗi thành số nguyên
    Serial.printf("Firebase: Task2 Minute (OFF) = %d\n", task2MinuteFirebase);
  } else {
    Serial.println("Failed to read Task2 Minute (OFF) from Firebase");
  }
}

// ======== Xử lý nhấn nút =========
void handleButton() {
  if (buttonPressed) {
    buttonPressed = false; // Xử lý xong thì xóa cờ

    unsigned long currentTime = millis();
    if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) { // Chống dội nút
      lastDebounceTime = currentTime;

      fanState = !fanState; // Đảo trạng thái quạt
      digitalWrite(FAN_PIN, fanState ? HIGH : LOW);

      // Ghi trạng thái mới lên Firebase
      String value = fanState ? "1" : "0";
      if (Firebase.RTDB.setString(&fbdo, "Control/FAN", value)) {
        Serial.printf("Button Pressed -> Updated FAN to %s\n", value.c_str());
      } else {
        Serial.println("Button Pressed -> Failed to update FAN state");
      }
    }
  }
}

// ======== Đọc trạng thái quạt từ Firebase =========
void readFanStateFromFirebase() {
  if (Firebase.RTDB.getString(&fbdo, "Control/FAN")) {
    String remoteState = fbdo.stringData();
    bool remoteFanState = (remoteState == "1");

    if (remoteFanState != fanState) {
      fanState = remoteFanState;
      digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
      Serial.printf("Firebase Updated -> Fan set to %s\n", remoteState.c_str());
    }
  } else {
    Serial.println("Failed to read FAN state from Firebase");
  }
}

// ======== Xử lý bật/tắt quạt theo lịch =========
void handleScheduledTask() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  Serial.printf("Current time: %02d:%02d:%02d, Date: %04d-%02d-%02d\n",
              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
              timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

  // Task 1: Bật quạt theo giờ từ Firebase
  if (timeinfo.tm_hour == task1HourFirebase && timeinfo.tm_min == task1MinuteFirebase && lastRunMinuteTask1 != timeinfo.tm_min) {
    fanState = true;
    digitalWrite(FAN_PIN, HIGH);
    Firebase.RTDB.setString(&fbdo, "Control/FAN", "1");
    Serial.println("######### Bật quạt tự động theo lịch Firebase #########");
    lastRunMinuteTask1 = timeinfo.tm_min;  // Cập nhật phút cuối cùng đã bật
  }

  // Task 2: Tắt quạt theo giờ từ Firebase
  if (timeinfo.tm_hour == task2HourFirebase && timeinfo.tm_min == task2MinuteFirebase && lastRunMinuteTask2 != timeinfo.tm_min) {
    fanState = false;
    digitalWrite(FAN_PIN, LOW);
    Firebase.RTDB.setString(&fbdo, "Control/FAN", "0");
    Serial.println("######### Tắt quạt tự động theo lịch Firebase #########");
    lastRunMinuteTask2 = timeinfo.tm_min;  // Cập nhật phút cuối cùng đã tắt
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