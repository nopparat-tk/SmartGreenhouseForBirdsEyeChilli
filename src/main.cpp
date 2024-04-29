#include <Arduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
// #include <Adafruit_GFX.h>
#include <Wire.h>
#include <AM2315C.h>
#include <ModbusMaster.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h>

/* ประกาศตัวแปร สำหรับ Weather Sensor  ***********/
AM2315C DHT;
uint8_t count = 0;
uint32_t stop, start;

float air_humi, air_temp; 

/* ประกาศอินสแต๊นซ์ Modbus สำหรับ Soil Sensor  ***********/
ModbusMaster myModbus;
#define SLAVE_ID 1                        // ประกาศตัวแปร SLAVE ID (ID ของตัวเซนเซอร์ที่ต้องการอ่าน)
#define SLAVE_BAUDRATE 9600               // ประกาศตัวแปร SLAVE BAUDRATE (อัตราความเร็วรับ-ส่งข้อมูล) *** ค่า Baud rate ต้องตรงกับตัวเซนเซอร์ที่เราจะอ่านค่า
/* ประกาศตัวแปร สำหรับ Soil Sensor  ***********/
float Temperature, Humidity, PHvalue;
uint16_t Nitrogen, Phosphorus, Potassium;

/* ประกาศตัวแปร สำหรับหน้าจอ TFT  ***********/
TFT_eSPI tft;

/* ประกาศตัวแปร สำหรับเชื่อมต่อ Wifi  ***********/
const char* ssid     = "Smart-Greenhouse";
const char* password = "MJU6415125007";
// const char* ssid     = "R-Home";
// const char* password = "8801019525";
// const char* ssid     = "R-iPhone";
// const char* password = "0621989619";

/* ประกาศตัวแปร สำหรับเชื่อมต่อ MQTT broker  ***********/
// const char* mqtt_server = "167.71.223.61";
// const int mqtt_port = 1883;
const char* mqtt_server = "139.59.237.181";
const int mqtt_port = 1883;
const char* mqtt_user = "tiny32";
const char* mqtt_password = "tiny32";
// const char* mqtt_server = "3f8c7ebf922c4ae39f9cf3f5d4a7a79b.s1.eu.hivemq.cloud";
// const int mqtt_port = 8883;
// const char* mqtt_user = "Smart-Greenhouse";
// const char* mqtt_password = "Mju6415125007";
const char* mqtt_topic = "test/v1";

WiFiClient espClient;
PubSubClient client(espClient);

/*  ประกาศตัวแปรการให้ Relay สำหรับระบบควบคุม  ***********/
int SOIL = 4;
int VENT = 13;
int FOGGY = 26;
// int R4 = 15;

/* ประกาศตัวแปร สำหรับ Weather Control  ***********/
int IsFoggyOpen = 0;
int IsVentOpen = 0;
int LowestBoundHumi = 20;   // ค่าเริ่มต้น 20
int LowerBoundHumi = 55;   // ค่าเริ่มต้น 60
int MiddleBoundHumi = 65;  // ค่าเริ่มต้น 65
int UpperBoundHumi = 80;   // ค่าเริ่มต้น 75
int LowerBoundTemp = 30;   // ค่าเริ่มต้น 28
int UpperBoundTemp = 34;   // ค่าเริ่มต้น 33

/* ประกาศตัวแปร สำหรับ Water Control  ***********/
int LowerBoundSoilHumi = 50;  // ค่าเริ่มต้น 60
int UpperBoundSoilHumi = 60;  // ค่าเริ่มต้น 65

const char* watering_state;
const char* foggy_state;
const char* vent_state;

/* ประกาศแยก Void การทำงาน  ***********/
void tft_Updated();
void wifi_Connect();
void Reconnect();
void vent_Control();
void foggy_Control();
void water_System();
void relay_State();
void publishJsonData();
void callback(char* topic, byte* payload, unsigned int length);


/***********   Void Setup  ***********/
void setup() {

   Serial.begin(9600);
   delay(1000);

   /*********** Set PIN Relay ***********/
   /****** HIGH = OFF, LOW = ON ******/ 
   pinMode(VENT, OUTPUT);
   pinMode(FOGGY, OUTPUT);
   pinMode(SOIL, OUTPUT);
   // pinMode(R4, OUTPUT); 

   /***********  Set Relay OFF ***********/
   digitalWrite(VENT, HIGH);
   digitalWrite(FOGGY, HIGH);
   digitalWrite(SOIL, HIGH);
   // digitalWrite(R4, HIGH);

   /***********  Start WiFi Connecting***********/
   wifi_Connect();

   /***********  Start TFT 4.0 inch Screen  ***********/
   tft.begin(); // เริ่มต้นใช้งาน tft
   tft.init();
   tft.setRotation(1); //1 = หน้าจอแนวนอน , 2 = หน้าจอแนวตั้ง
   tft.fillScreen(TFT_BLACK); //โชว์หน้าจอสีดำ
   tft.setTextFont(4);
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_YELLOW); //สีตัวหนังสือ
   tft.setCursor(120, 148); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Smart Greenhouse"); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_WHITE); //สีตัวหนังสือ
   tft.setCursor(116, 172); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("wifi connecting ...");

   /***********  Weather Sensor  ***********/
   DHT.begin();    //  ESP32 default pins 21 22
   // Serial.begin(9600);
   Serial.println(__FILE__);
   Serial.print("AM2315C LIBRARY VERSION: ");
   Serial.println(AM2315C_LIB_VERSION);
   Serial.println();
   delay(1000);

   /***********  Soil Sensor  ***********/
   myModbus.begin(SLAVE_ID, Serial2);
   // Serial.begin(9600);
   Serial2.begin(SLAVE_BAUDRATE, SERIAL_8N1, 16, 17);     // เริ่มการเปิดพอร์ตสื่อสาร Serial2, ค่าความเร็วสื่อสารที่ 4760 (ตั้งค่าให้ตรงกับเซนเซอร์ที่จะอ่าน), Data bit 8: parity: NONE, Stopbit :1, ขา 16 เป็น RX, ขา 17 เป็น TX

}

/***********   Void loop   ***********/
void loop() {
   if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected to ");
      Serial.print(ssid);
      Serial.print(" : ");
      Serial.print(WiFi.localIP());
      Serial.print(" /");
      Serial.print("\n");
      }
   else {
      Serial.print("WiFi not connected!");
   }

   /***********  MQTT Connection ***********/
   if (!client.connected()) {
      // Reconnect to MQTT broker
      Reconnect();
   }
   
   // Publish a message to a topic
   client.loop();

   // Wait for a few seconds
   delay(1000);

   /***********  MQTT Connection ***********/

   /***********  Weather Sensor   ***********/
   // float Humi = DHT.getHumidity();
   // float Temp = DHT.getTemperature();
   air_humi = DHT.getHumidity();
   air_temp = DHT.getTemperature();
      if (millis() - DHT.lastRead() >= 5000) {
      //  READ DATA
      start = micros();
      int status = DHT.read();
      stop = micros();

      Serial.println();
      Serial.println("Type\t    Humidity\t   Temperature\t     Time (TH)\t    Status");
      Serial.print("AM2315C\t    ");
      Serial.printf("%.1f %cRH", air_humi, 37);  //  DISPLAY DATA, sensor has only one decimal. แสดงผลค่า Sensor ด้วย ทศนิยม 1 ตำแหน่ง
      Serial.print("\t     ");
      Serial.printf("%.1f °C", air_temp);
      Serial.print("\t    ");
      Serial.print(start - stop);
      Serial.print("\t      ");
      switch (status) {
         case AM2315C_OK:
         Serial.print("OK");
         break;
         case AM2315C_ERROR_CHECKSUM:
         Serial.print("Checksum error");
         break;
         case AM2315C_ERROR_CONNECT:
         Serial.print("Connect error");
         break;
         case AM2315C_MISSING_BYTES:
         Serial.print("Missing bytes");
         break;
         case AM2315C_ERROR_BYTES_ALL_ZERO:
         Serial.print("All bytes read zero");
         break;
         case AM2315C_ERROR_READ_TIMEOUT:
         Serial.print("Read time out");
         break;
         case AM2315C_ERROR_LASTREAD:
         Serial.print("Error read too fast");
         break;
         default:
         Serial.print("Unknown error");
         break;
      }
      Serial.print("\n\n");
   }
   /***********  Weather Sensor   ***********/

   /***********  Soil Sensor  ***********/
   uint8_t result;
      // float Temperature, Humidity, PHvalue, Nitrogen, Phosphorus, Potassium;

      result = myModbus.readHoldingRegisters(0, 7);   // เริ่มอ่านค่าที่ตำแหน่งรีจิสเตอร์ 0, เป็นจำนวน 7 รีจิสเตอร์

      if (result == myModbus.ku8MBSuccess) {    // หากสำเร็จ เซนเซอร์ตอบกลับ และไม่มีผิดพลาด
         Humidity = (myModbus.getResponseBuffer(0) / 10.0) * 3;   // เอาค่า Buffer 0 ที่อ่านจาก Modbus มาไว้ในตัวแปร Humidity และหารด้วย 10
         Temperature = myModbus.getResponseBuffer(1) / 10.0;      // เอาค่า Buffer 1 ที่อ่านจาก Modbus มาไว้ในตัวแปร Temperature และหารด้วย 10
         PHvalue = myModbus.getResponseBuffer(3) / 10.0;          // เอาค่า Buffer 3 ที่อ่านจาก Modbus มาไว้ในตัวแปร PH Value และหารด้วย 10
         Nitrogen = myModbus.getResponseBuffer(4);                // เอาค่า Buffer 5 ที่อ่านจาก Modbus มาไว้ในตัวแปร Nitrogen 
         Phosphorus = myModbus.getResponseBuffer(5);              // เอาค่า Buffer 6 ที่อ่านจาก Modbus มาไว้ในตัวแปร Phosphorus 
         Potassium = myModbus.getResponseBuffer(6);               // เอาค่า Buffer 7 ที่อ่านจาก Modbus มาไว้ในตัวแปร Potassium 
      }

      Serial.println("Type\t\t  Humidity\t   Temperature       PHvalue       Nitrogen\t  Phosphorus\t    Potassium");
      Serial.print("Soil Sensor\t");
      Serial.printf ("Humi: %.1f %cRH \t  ", Humidity, 37);
      Serial.printf ("Temp: %.1f °C      ", Temperature);
      Serial.printf ("pH: %.1f     ", PHvalue);
      Serial.printf ("N: %d mg/Kg \t  ", Nitrogen);
      Serial.printf ("P: %d mg/Kg \t   ", Phosphorus);
      Serial.printf ("K: %d mg/Kg \t", Potassium);
      Serial.print("\n\n");
   /***********  Soil Sensor  ***********/

   /***********  Environmental control  ***********/
   water_System();
   vent_Control();
   foggy_Control();
   Serial.print("\n\n");
   /***********  Environmental control  ***********/

   /***********  tft update  ***********/
   tft_Updated();
   /***********  tft update  ***********/

   /***********  MQTT Data  ***********/
   publishJsonData();
   Serial.print("\n\n");
   /***********  MQTT Data  ***********/

   // Wait for a few seconds
   delay(10000);
}
/***********   END Loop   ***********/


/***********   Void Check relay_State   ***********/
void relay_State(int STATUS, int NUMBER) {
   if (STATUS == 0) {
         if (NUMBER == 1) {
            watering_state = "ON";
         }   
         else if (NUMBER == 2) {
            vent_state = "ON";
         }
         else {
            foggy_state = "ON";
         }
      }
      else {
         if (NUMBER == 1) {
            watering_state = "OFF";
         }   
         else if (NUMBER == 2) {
            vent_state = "OFF";
         }
         else {
            foggy_state = "OFF";
         }
      }
}
/***********   END   ***** Check relay_State  ***********/


/***********   Void water_System   ***********/
void water_System() {

   if (Humidity < LowerBoundSoilHumi) {         // เช็คความชื้นในดิน ถ้า <= 60%RH
         digitalWrite(SOIL, LOW);               // เปิด ระบบจ่ายน้ำพืช
      }
      else if (Humidity > UpperBoundSoilHumi) { // เช็คความชื้นถ้า >= 70%RH
         digitalWrite(SOIL, HIGH);              // ปิด ระบบจ่ายน้ำพืช
      }
      else {
         Serial.print("Irrigation Problem");
      }

   relay_State(digitalRead(SOIL),1);

   Serial.print("WATER State : ");
   Serial.print(watering_state);
   Serial.print(" ");
}
/***********   END   *****   water_System  ***********/


/***********  void vent_Control  ***********/
void vent_Control() {
   // float air_humi = DHT.getHumidity();
   // float air_temp = DHT.getTemperature();

   if ((air_temp > UpperBoundTemp) || (air_humi > UpperBoundHumi)) {
      digitalWrite(VENT, LOW); 
   } else {
      digitalWrite(VENT, HIGH);
   }
   relay_State(digitalRead(VENT),2);
   relay_State(digitalRead(FOGGY),3);

   Serial.print("VENT State: ");
   Serial.print(vent_state);
   Serial.print(" ");
}
/***********   END   *****   vent_Control  ***********/

/***********  void foggy_Control  ***********/
void foggy_Control() {
   // float air_humi = DHT.getHumidity();
   // float air_temp = DHT.getTemperature();

   relay_State(digitalRead(VENT),2);
   relay_State(digitalRead(FOGGY),3);

   if (air_humi > LowestBoundHumi && air_humi < LowerBoundHumi) {
      digitalWrite(FOGGY, LOW); 
   } else if (air_humi > MiddleBoundHumi) {
      digitalWrite(FOGGY, HIGH);
   } else if (air_humi < LowestBoundHumi) {
      digitalWrite(FOGGY, HIGH);
   }
   Serial.print("FOGGY State: ");
   Serial.print(foggy_state);
   Serial.print(" ");
}
/***********   END   *****   foggy_Control  ***********/

/***********  tft_Updated   ***********/
void tft_Updated() {
   
   tft.fillScreen(0x0000);
   tft.setTextFont(4);
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_YELLOW); //สีตัวหนังสือ
   tft.setCursor(16, 8); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Smart Greenhouse : ");
   
   /***********  WiFI Connect   ***********/
   if (WiFi.status() == WL_CONNECTED) {
      tft.setTextFont(0); // รูปแบบตัวหนังสือ
      tft.setTextSize(2); // ไซส์ตัวหนังสือ
      tft.setCursor(238, 13);
      tft.setTextColor(TFT_YELLOW);
      tft.print(" WiFi connected");
      // tft.print(ssid);
      // tft.print(" : ");
      // tft.print(WiFi.localIP());
   } else {
      tft.setTextColor(0xef5d);
      tft.print(" WiFi not connect!");
   }

   /***********  Weather Sensor   ***********/
   // float Humi = DHT.getHumidity();
   // float Temp = DHT.getTemperature();
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0xfddf); //สีตัวหนังสือ
   tft.setCursor(16, 40); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Ambient Sensor :"); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(16, 70); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Humi: "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0xfddf); //สีตัวหนังสือ
   tft.printf("%.1f", air_humi, 37);
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print(" %RH  ");
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print("Temp: "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0xfddf); //สีตัวหนังสือ
   tft.printf("%.1f", air_temp);
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print(" C");

   /***********  Soil Sensor  ***********/
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.setCursor(16, 106); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Soil Sensor :"); // ปริ้นท์ตัวหนังสือ
   tft.setCursor(16, 132); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Quality"); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(18,162); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Humi: "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.printf("%.1f", Humidity, 37);
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print(" %RH");
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(16,188); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Temp: "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.printf("%.1f", Temperature);
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print(" C");
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(16,212); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("pH  : "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.printf("%.1f", PHvalue);

   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setCursor(240, 132); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.print("Nutrients "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(0); // ไซส์ตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(344, 136); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("(mg/Kg)");
   tft.setTextFont(0); // ไซส์ตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setCursor(242,162); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print("Nitrogen  : "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.print(Nitrogen);
   tft.setTextFont(0);
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(242,188); // ไซส์ตัวหนังสือ
   tft.print("Phosphorus: "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.print(Phosphorus);
   tft.setTextFont(0); // ไซส์ตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setCursor(242,212); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.print("Potassium : "); // ปริ้นท์ตัวหนังสือ
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.print(Potassium);

   /***********  Control System  ***********/
   tft.setTextFont(4); // รูปแบบตัวหนังสือ
   tft.setTextSize(1); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_YELLOW); //สีตัวหนังสือ
   tft.setCursor(16,252); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Control System");
   tft.setTextFont(0); // รูปแบบตัวหนังสือ
   tft.setTextSize(2); // ไซส์ตัวหนังสือ
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(16,284); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("WATER: ");
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.setCursor(92,285); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print(watering_state);
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(160,284); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("FOGGY: ");
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.setCursor(232,285); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print(foggy_state);
   tft.setTextColor(TFT_CYAN); //สีตัวหนังสือ
   tft.setCursor(312,284); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print("Vent: ");
   tft.setTextColor(0x0760); //สีตัวหนังสือ
   tft.setCursor(372,285); //ตำแหน่ง x,y ที่จะให้โชว์ตัวหนังสือ
   tft.print(vent_state);
}
/***********   END   *****   tft_Updated   ***********/


/***********   wifi Connection   ***********/
void wifi_Connect() {
   Serial.print("Connecting to : ");
   Serial.println(ssid);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      Serial.print(".");
   }
   Serial.println("");
   Serial.println("WiFi connected");

   // Connect to MQTT broker
   client.setServer(mqtt_server, mqtt_port);
   client.connect(mqtt_topic, mqtt_user, mqtt_password);
   client.setCallback(callback);
   client.subscribe("test/v1");
   // client.setCredentials(mqtt_user, mqtt_password);

   // Start the server
   //   server.begin();
   //   Serial.println("Server started");
   //   // Print the IP address
   //   Serial.print("Use this URL to connect: ");
   //   Serial.print("http://");
   Serial.print(ssid);
   Serial.print(" : ");
   Serial.print(WiFi.localIP());
   Serial.println("/");
   Serial.println();
} 
/***********   END   *****   wifi Connection   ***********/  


/***********   MQTT Reconnect   ***********/
void Reconnect() {
   while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      //*** Attempt to connect ***
      if (client.connect(mqtt_topic, mqtt_user, mqtt_password)) {
         // mcu.TickBuildinLED(1.0);
         client.subscribe("test/v1");
         Serial.println("connected");
         Serial.println("-------------------------------");
         Serial.printf("Info: Sending data to MQTT server ...");
         Serial.print("\n");
      } else {
         // mcu.TickBuildinLED(0.1);
         // if (_cnt_server++ > 5)
         //     break;
         Serial.print("failed, rc=");
         Serial.print(client.state());
         Serial.println(" try again in 5 seconds");
         // Wait 5 seconds before retrying
         delay(5000);
      }
   }
}
/***********  END  *****  MQTT Reconnect  ***********


/***********  MQTT Subscribtion  ***********/
void callback(char* topic, byte* payload, unsigned int length) {
   payload[length] = '\0';
   String topic_str = topic, payload_str = (char*)payload;
   Serial.println("[" + topic_str + "]: " + payload_str);

   // digitalWrite(LED_PIN, (payload_str == "ON") ? HIGH : LOW);
}
/***********  END  *****  MQTT Subscribtion   ***********/


/***********   MQTT Publish Json Data   ***********/
void publishJsonData() {
   // Create a JSON document
   DynamicJsonDocument jsonDocument(200);

   // Fill the JSON document
   // jsonDocument["sensor"] = "temperature";
   // jsonDocument["value"] = 25.5;
   // jsonDocument["Sensor1"] = "Weather";
   jsonDocument["air_temp"] = roundf(DHT.getTemperature() * 10.0) / 10.0;
   jsonDocument["air_humi"] = roundf(DHT.getHumidity() * 10.0) / 10.0;
   // jsonDocument["Soil"]; // = "Soil";
   jsonDocument["soil_humi"] = (myModbus.getResponseBuffer(0) / 10.0) * 3;
   jsonDocument["soil_temp"] = myModbus.getResponseBuffer(1) / 10.0;
   jsonDocument["phvalue"] = myModbus.getResponseBuffer(3) / 10.0;
   jsonDocument["nitrogen"] = myModbus.getResponseBuffer(4);
   jsonDocument["phosphorus"] = myModbus.getResponseBuffer(5);
   jsonDocument["potassium"] = myModbus.getResponseBuffer(6);
   jsonDocument["watering_state"] = watering_state;
   jsonDocument["foggy_state"] = foggy_state;
   jsonDocument["vent_state"] = vent_state;

   // Serialize the JSON document to a string
   String jsonString;
   serializeJson(jsonDocument, jsonString);

   // Publish the JSON string to an MQTT topic
   client.publish("test/v1", jsonString.c_str());
}
/***********  END  *********** MQTT Publish Json Data ***********/
