#include <HardwareSerial.h>

HardwareSerial zigbeeSerial(1);

// Đánh dấu các task
TaskHandle_t sendData_Task;
TaskHandle_t rcv_push_button_Task;

//Bien check co dang suspend hay chua
volatile bool suspend = false;

// Phần này dành cho các peripheral   
#define LDR 39
#define D_AO_NUOI 2   
#define FAN 18
#define OXI 19
#define MSP 5
#define MRTA 15
#define MAY_BOM_VAO 4
#define MAY_BOM_RA 0 

// Phần này dành cho TDS
const int TDS_Sensor_pin = 32;
const float VREF = 3.3;                                                                                                         
const int SCOUNT = 10; // Số mẫu cần thu thập
int arr[SCOUNT];
int index_arr = 0;
float temp = 28.0; // Giả định nhiệt độ là 28 độ C

//Phần này dành cho cảm biến mức nước 
#define sensorPower 12
#define sensorPin 14
int val = 0;

//Phần này dành cho cảm biến PH
#define PH_PIN 27


void setup() {
  Serial.begin(9600);
  zigbeeSerial.begin(115200, SERIAL_8N1, 16, 17);  

  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, 0);                                
  pinMode(OXI, OUTPUT);
  digitalWrite(OXI, 0);
  pinMode(MSP, OUTPUT);
  digitalWrite(MSP, 0);
  pinMode(MRTA, OUTPUT);
  digitalWrite(MRTA, 0);

  pinMode(MAY_BOM_VAO, OUTPUT);
  digitalWrite(MAY_BOM_VAO, 0);

  pinMode(MAY_BOM_RA, OUTPUT);
  digitalWrite(MAY_BOM_RA, 0);

  pinMode(LDR, INPUT); 
  pinMode(D_AO_NUOI, OUTPUT);
  digitalWrite(D_AO_NUOI, 0);

  pinMode(PH_PIN, INPUT);
  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);
  
  xTaskCreatePinnedToCore(
        rcv_push_button,
        "rcv_push_button",
        4000,
        NULL,
        1,
        &rcv_push_button_Task,
        0 // Ghim vào core 0
  );

  xTaskCreatePinnedToCore(
    sendData,
    "Send_DATA_TASK",
    16384,
    NULL,
    2, 
    &sendData_Task,
    1 
  );
}

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

float water_sensor(){
  float level = readSensor();
  level = (level/1450) * 100;
  return level;
}

int readSensor() {
    digitalWrite(sensorPower, HIGH);    
    delay(10);                 
    val = analogRead(sensorPin);        
    digitalWrite(sensorPower, LOW);        
    return val;                 
}

float TDS_Cal() {
  if (index_arr < SCOUNT) {
    arr[index_arr] = analogRead(TDS_Sensor_pin);
    index_arr++;
    return -1; 
  } else {
    int arrTemp[SCOUNT];
    for (int i = 0; i < SCOUNT; i++) {
      arrTemp[i] = arr[i];
    }

    float avrg_voltage = getMedianNum(arrTemp, SCOUNT) * VREF / 4095.0;
    float he_so_den_bu = 1.0 + 0.02 * (temp - 25.0);
    float bu_dien_ap = avrg_voltage / he_so_den_bu;
    float TDS_Val = (133.42 * bu_dien_ap * bu_dien_ap * bu_dien_ap 
                     - 255.86 * bu_dien_ap * bu_dien_ap 
                     + 857.39 * bu_dien_ap) * 0.5;
    
    index_arr = 0;
    return TDS_Val;
  }
}

float readPH() {
    int rawValue = analogRead(PH_PIN);  
    float voltage = (rawValue / 4095.0) * 3.3; 
    return 7 + ((2.50 - voltage) / 0.18);  
}

int LDR_Cal(){
  int ldr;
  return ldr = analogRead(LDR);
}

void sendData(void *pvParameters) {
  bool may_bom_vao_on = false;
  bool may_bom_ra_on = false;
  bool den_ao_nuoi_on = false;

  while (true) {
    if (!suspend) {
      float level = water_sensor();
      int ldr = LDR_Cal();

      if (level < 20.0 && !may_bom_vao_on) {
        Serial.println("MAY_BOM_VAO ON");
        digitalWrite(MAY_BOM_VAO, 1);
        digitalWrite(MAY_BOM_RA, 0);
        zigbeeSerial.println("MBV_ON");
        may_bom_vao_on = true;
        may_bom_ra_on = false;
      } else if (level >= 30.0 && may_bom_vao_on) {
        Serial.println("MAY_BOM_VAO OFF");
        digitalWrite(MAY_BOM_VAO, 0);
        digitalWrite(MAY_BOM_RA, 0);
        zigbeeSerial.println("MBV_OFF");
        may_bom_vao_on = false;
        may_bom_ra_on = false;
      }

      // Điều khiển máy bơm ra
      if (level > 80.0 && !may_bom_ra_on) {
        Serial.println("MAY_BOM_RA ON");
        digitalWrite(MAY_BOM_RA, 1);
        digitalWrite(MAY_BOM_VAO, 0);
        zigbeeSerial.println("MBR_ON");
        may_bom_ra_on = true;
        may_bom_vao_on = false;
      } else if (level <= 70.0 && may_bom_ra_on) {
        Serial.println("MAY_BOM_RA OFF");
        digitalWrite(MAY_BOM_RA, 0);
        digitalWrite(MAY_BOM_VAO, 0);
        zigbeeSerial.println("MBR_OFF");
        may_bom_ra_on = false;
        may_bom_vao_on = false;
      }

      if (ldr > 2700 && !den_ao_nuoi_on) {
        Serial.println("DEN_AO_NUOI ON");
        digitalWrite(D_AO_NUOI, 1);
        zigbeeSerial.println("D_AN_ON");
        den_ao_nuoi_on = true;
      } else if (ldr <= 2700 && den_ao_nuoi_on) {
        Serial.println("DEN_AO_NUOI OFF");
        digitalWrite(D_AO_NUOI, 0);
        zigbeeSerial.println("D_AN_OFF");
        den_ao_nuoi_on = false;
      }
      vTaskDelay(200 / portTICK_PERIOD_MS); 

      float tds_val = TDS_Cal();
      if (tds_val > -1) {
        zigbeeSerial.print("TDS_Value: ");
        zigbeeSerial.print(tds_val);
        zigbeeSerial.println(" ppm");
      }

      zigbeeSerial.print("Water_Level: ");
      zigbeeSerial.print(level);
      zigbeeSerial.println(" %");

      float ph_val = readPH();
      zigbeeSerial.print("PH_Value: ");
      zigbeeSerial.print(ph_val);
      zigbeeSerial.println(" ph");
      vTaskDelay(1500 / portTICK_PERIOD_MS);
    }
  }
}

void rcv_push_button(void *pvParameters) {
  while (true) {
    if (zigbeeSerial.available() > 0) {
      String data = zigbeeSerial.readStringUntil('\n');
      data.trim();
      Serial.println("Received data: " + data);  

      // if (data == "SUSPEND_DATA") {
      //   suspend = true;
      //   vTaskSuspend(sendData_Task);
      //   //vTaskSuspend(rcv_push_button_Task);
      // }
      // if (data == "RESUME_DATA") {
      //   suspend = false;
      //   vTaskResume(sendData_Task);
      //   //vTaskResume(rcv_push_button_Task);
      // }
      
      if (data == "FAN_ON") {
        digitalWrite(FAN, 1);
        zigbeeSerial.println("ACK_FAN_ON");
        Serial.println("QUAT_ON");  
      } else if (data == "FAN_OFF") {
        digitalWrite(FAN, 0);
        zigbeeSerial.println("ACK_FAN_OFF");
        Serial.println("QUAT_OFF");
      } else if (data == "OXI_ON") {
        digitalWrite(OXI, 1);
        zigbeeSerial.println("ACK_OXI_ON");
        Serial.println("OXI_ON");
      } else if (data == "OXI_OFF") {
        digitalWrite(OXI, 0);
        zigbeeSerial.println("ACK_OXI_OFF");
        Serial.println("OXI_OFF");
      } else if (data == "MSP_ON") {
        digitalWrite(MSP, 1);
        zigbeeSerial.println("ACK_MSP_ON");
      } else if (data == "MSP_OFF") {
        digitalWrite(MSP, 0);
        zigbeeSerial.println("ACK_MSP_OFF");
      } else if (data == "MRTA_ON") {
        digitalWrite(MRTA, 1);
        zigbeeSerial.println("ACK_MRTA_ON");
      } else if (data == "MRTA_OFF") {
        digitalWrite(MRTA, 0);
        zigbeeSerial.println("ACK_MRTA_OFF");
      } else if (data == "MBV_ON") {
        digitalWrite(MAY_BOM_VAO, 1);
        zigbeeSerial.println("ACK_MBV_ON");
      } else if (data == "MBV_OFF") {
        digitalWrite(MAY_BOM_VAO, 0);
        zigbeeSerial.println("ACK_MBV_OFF");
      } else if (data == "MBR_ON") {
        digitalWrite(MAY_BOM_RA, 1);
        zigbeeSerial.println("ACK_MBR_ON");
      } else if (data == "MBR_OFF") {
        digitalWrite(MAY_BOM_RA, 0);
        zigbeeSerial.println("ACK_MBR_OFF");
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void loop() {
  vTaskDelay(10);
}