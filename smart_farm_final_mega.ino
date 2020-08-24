#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ModbusMaster.h>
#include <SHT2x.h>
#include "SHT2x.h"
#include <Average.h>
#include <ArduinoJson.h>

#include <avr/wdt.h>
#include <Ethernet.h>
#include <ArduinoOTA.h>
#include <HttpClient.h>

ModbusMaster node;
#define SCREEN_WIDTH 128 // pixel ความกว้าง
#define SCREEN_HEIGHT 64 // pixel ความสูง
#define RS485Enable_pin 7
#define RS485Transmit    HIGH
#define RS485Receive     LOW
// กำหนดขาต่อ I2C กับจอ OLED
#define OLED_RESET     -1 //ขา resfet เป็น -1 ถ้าใช้ร่วมกับขา Arduino reset
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//------- RELAY
#define Relay1 23
#define Relay2 27
#define Relay3 31
#define SSR1 39
#define SSR2 43

int Relay1_status = 0;
int Relay2_status = 0;
int Relay3_status = 0;
int ssr1_status = 0;
int ssr2_status = 0;
extern "C" char* sbrk(int incr);
int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

SHT2x SHT2x;


// ประกาศตัวแปลเก็บ pin ที่เป็น หลอดไฟ RGB
int RGB[] = {10, 11, 12};
bool checkhotspot = 0;
int checklosswifi = 0;
int interval_read = 1000 * 6; // อ่านทุกๆ 6 วินาที -----------------------  ****** TIME
unsigned int interval_send = 1000 * 30; // ส่งทุกๆ 60 วินาที ----------------------- ****** TIME
String mac;
String wifiname;
unsigned long CurrentTime = 0;
unsigned long PreviousTime_read = 1000 * 13; // 13 วินาทีเผื่อให้ esp boot wifimanager เสร็จ
unsigned long PreviousTime_send = 1000 * 20; // 20 วินาทีเผื่อให้ esp พร้อม publish ไป broker
const int SlaveAddress = 0x01;
const int Modbusfunction = 0x03; // Read Multiple Holding Registers



Average<float> humidity(5);
Average<float> temperature(5);
Average<float> soilhumidity(5);

uint16_t ADC_Soil[10];
uint16_t ADC_check[] = {1, 3, 4};
float ADC_SoilMoisture = 0;
bool ReceiveedData = false;
int count = 0;
bool checksensor = false;


void SetRGB(int b, int g, int r) {
  digitalWrite(RGB[0], b);
  digitalWrite(RGB[1], g);
  digitalWrite(RGB[2], r);
}
//For update
void handleSketchDownload(long filesize) {
  const unsigned long CHECK_INTERVAL = 590000;
  static unsigned long previousMillis;

  if (!InternalStorage.open(filesize)) {
//    client.stop();
    Serial.println("There is not enough space to store the update. Can't continue with update.");
    return;
  }
  Serial.println("Opened internal storage!");
// maxmimum time ??
  previousMillis = millis();
  while (true) {
    wdt_reset();
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > CHECK_INTERVAL) {
      Serial.println("time out -- finished!?");
      InternalStorage.close();
      Serial.println("Sketch update apply and reset.");
      Serial.flush();
      InternalStorage.apply(); // this doesn't return
      break;
    }
    if (Serial3.available()) {
      int x = Serial3.read();
      Serial.write(x);
      InternalStorage.write(x);
    }
  }
}

void showoled() {
//ใส่ค่า sensor เป็นเทมเพลตเลย
//  Serial.print ("Free memory is: ");
//  Serial.println (freeMemory ());
  OLED.clearDisplay(); // ลบภาพในหน้าจอทั้งหมด
  OLED.setTextColor(WHITE, BLACK);  //กำหนดข้อความสีขาว ฉากหลังสีดำ
  OLED.setTextSize(2); // กำหนดขนาดตัวอักษร
  OLED.setCursor(0, 0); // กำหนดตำแหน่ง x,y ที่จะแสดงผล
  OLED.println("Smart Farm");
  OLED.setTextSize(1); // กำหนดขนาดตัวอักษร
  OLED.print("Mac : ");
  OLED.println(mac);
  if (checklosswifi == 1) {
    OLED.setTextColor(BLACK, WHITE);
    OLED.println("       Lost wifi     ");
    OLED.setTextColor(WHITE, BLACK);
  } else {
    OLED.print("Wifi : ");
    OLED.println(wifiname);
  }
  OLED.print("AirTemp : ");
  OLED.print(temperature.mean());
  OLED.println(" C");
  OLED.print("AirMois : ");
  OLED.print(humidity.mean());
  OLED.println(" %");
  OLED.print("SoilMois : ");
  if (int(soilhumidity.mean()) == 0) {
    OLED.print("0.00");
  } else {
    OLED.print(soilhumidity.mean());
  }
  OLED.println(" %");
  OLED.print("Relay : ");
  OLED.print("[");
  OLED.print(ssr1_status);
  OLED.print(",");
  OLED.print(ssr2_status);
  OLED.print(",");
  OLED.print(Relay1_status);
  OLED.print(",");
  OLED.print(Relay2_status);
  OLED.print(",");
  OLED.print(Relay3_status);
  OLED.print("]");
  OLED.display(); // สั่งให้จอแสดงผล
}
void showoled_hospot() {
  //ใส่ค่า sensor เป็นเทมเพลตเลย
  OLED.clearDisplay(); // ลบภาพในหน้าจอทั้งหมด
  OLED.setTextColor(WHITE, BLACK);  //กำหนดข้อความสีขาว ฉากหลังสีดำ
  OLED.setTextSize(1); // กำหนดขนาดตัวอักษร
  OLED.setCursor(0, 0); // กำหนดตำแหน่ง x,y ที่จะแสดงผล
  OLED.print("Mac : ");
  OLED.println(mac);
  OLED.println("          ");
  OLED.setTextColor(BLACK, WHITE);
  OLED.setTextSize(1);
  OLED.println("                    ");
  OLED.setTextSize(2);
  OLED.println("  HOTSPOT ");
  OLED.setTextSize(1);
  OLED.println("                    ");
  OLED.println("      not found     ");
  if (wifiname.length() < 20) {
    String x = "";
    if (wifiname.length() % 2 == 0) {
      for (int i = 0; i < ((20 - wifiname.length()) / 2); i++) {
        x += ' ';
      }
      OLED.print(x);
      OLED.print(wifiname);
      OLED.println(x);
    } else {
      for (int i = 0; i < ((19 - wifiname.length()) / 2); i++) {
        x += ' ';
      }
      OLED.print(' ' + x);
      OLED.print(wifiname);
      OLED.println(x);
    }
  }
  OLED.println(wifiname);
  OLED.display(); // สั่งให้จอแสดงผล
}
void showoled_update() {
//ใส่ค่า sensor เป็นเทมเพลตเลย
  OLED.clearDisplay(); // ลบภาพในหน้าจอทั้งหมด
  OLED.setTextColor(WHITE, BLACK);  //กำหนดข้อความสีขาว ฉากหลังสีดำ
  OLED.setTextSize(1); // กำหนดขนาดตัวอักษร
  OLED.setCursor(0, 0); // กำหนดตำแหน่ง x,y ที่จะแสดงผล
  OLED.print("Mac : ");
  OLED.println(mac);
  OLED.println("          ");
  OLED.setTextColor(BLACK, WHITE);
  OLED.setTextSize(1);
  OLED.println("                    ");
  OLED.setTextSize(2);
  OLED.println("  UPDATE! ");
  OLED.setTextSize(1);
  OLED.println("                    ");
  OLED.println(" updating  firmware ");
  OLED.println("                    ");
  OLED.display(); // สั่งให้จอแสดงผล
}
//ฟังชั่นเปิดไฟ สี เขียน แดง น้ำเงิน
void greenled() {
  SetRGB(1, 0, 1);
}
void redled() {
  SetRGB(1, 1, 0);
}
void blueled() {
  SetRGB(0, 1, 1);
}

void setup() {
  wdt_enable(WDTO_8S);
  SHT2x.begin();
  Wire.begin();
  pinMode(RS485Enable_pin, OUTPUT);
  digitalWrite(RS485Enable_pin, RS485Transmit);
  Serial.begin(115200);
  Serial2.begin(9600);
  Serial3.begin(115200);
  pinMode(RGB[0], OUTPUT);
  pinMode(RGB[1], OUTPUT);
  pinMode(RGB[2], OUTPUT);
  redled();
  if (!OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // สั่งให้จอ OLED เริ่มทำงานที่ Address 0x3C
    Serial.println("SSD1306 allocation failed");
  } else {
    Serial.println("ArdinoAll OLED Start Work !!!");
    OLED.clearDisplay(); // ลบภาพในหน้าจอทั้งหมด
    OLED.setTextColor(WHITE, BLACK);  //กำหนดข้อความสีขาว ฉากหลังสีดำ
//    OLED.setCursor(0, 0); // กำหนดตำแหน่ง x,y ที่จะแสดงผล
    OLED.setTextSize(2); // กำหนดขนาดตัวอักษร
    OLED.println("Starting.."); // แสดงผลข้อความ ALL
    OLED.println(" ");
    OLED.println("Smart Farm");
    OLED.display(); // สั่งให้จอแสดงผล
  }
// Set all relay close at start
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  pinMode(SSR1, OUTPUT);
  pinMode(SSR2, OUTPUT);
  digitalWrite(Relay1, LOW);
  digitalWrite(Relay2, LOW);
  digitalWrite(Relay3, LOW);
  digitalWrite(SSR1, LOW);
  digitalWrite(SSR2, LOW);
}

void loop() {
  checksensor = false;
//  unsigned long checkworktime=millis();
  Relay1_status = digitalRead(Relay1);
  Relay2_status = digitalRead(Relay2);
  Relay3_status = digitalRead(Relay3);
  ssr1_status   = digitalRead(SSR1);
  ssr2_status   = digitalRead(SSR2);

  CurrentTime = millis();
//อ่าน Serial3 ที่มาจาก ESP แล้วหาว่ามีคำว่าอะไรบ้างแล้วนำไปทำอะไรต่อเป็นเงื่อนไขๆ
//  Serial.println(CurrentTime);
//  Serial.println(PreviousTime_read);
  //=============================== READ SENSOR =================================
  
  if ((CurrentTime >= PreviousTime_read + interval_read) && checkhotspot == 0) {
//  if (CurrentTime >= PreviousTime_read + interval_read) {

    // -------- READ SOIL --------------------------------------------------------------
    byte data[] = {SlaveAddress, Modbusfunction, 0x00, 0x01, 0x00, 0x02, 0x95, 0xCB}; // Volumetric water content rawl AD Value(VWCRAWAD) Wet less dry More
    digitalWrite(RS485Enable_pin, RS485Transmit);
    Serial2.begin(9600);
    Serial2.flush();
    Serial2.write(data, sizeof(data));
//    Serial.println("request sent");
    delay(10);
    digitalWrite(RS485Enable_pin, RS485Receive);

    // -------- READ AIR ----------------------------------------------------------------
    uint16_t rawh = SHT2x.readSensor(TRIGGER_HUMD_MEASURE_HOLD);
    uint16_t rawt = SHT2x.readSensor(TRIGGER_TEMP_MEASURE_HOLD);

    if (rawh <= ERROR_TIMEOUT or rawt <= ERROR_TIMEOUT) {
      if (rawh <= ERROR_TIMEOUT) {
        Serial.println("Error read humidity");
      }
      if (rawt <= ERROR_TIMEOUT) {
        Serial.println("Error read temperature");
      }
    } else {
//    for (int i = 0; i < 5; i++) {
//    Serial.print(humidity.get(i));
//    Serial.print(" ");
//    }
//    Serial.println("");
      humidity.push(SHT2x.GetHumidity());
      temperature.push(SHT2x.GetTemperature());
      checksensor = true;
      Serial.print("Humidity(%RH): ");
      Serial.print(humidity.mean());
      Serial.print("\tTemperature(C): ");
      Serial.print(temperature.mean());
    }
    showoled();
    PreviousTime_read = CurrentTime;
  }
  if (ReceiveedData == true)
  {
    if (count >= 8)
    {
      if (ADC_Soil[2] == 4) // Check data Length
      {
        ADC_SoilMoisture = (float)((ADC_Soil[3] << 8) + ADC_Soil[4]) / 100;
        soilhumidity.push(ADC_SoilMoisture);
        Serial.println("   Soil Moisture (%) = " + String(ADC_SoilMoisture));
        for (uint8_t i = 0; i < count; i++) {
          Serial.print(ADC_Soil[i], HEX);
          Serial.print(" ");
        }
        Serial.print("\n");
      }
    }
    memset(ADC_Soil, 0, sizeof(ADC_Soil));
    ReceiveedData = false;
    count = 0;
//    int soilint = int(ADC_SoilMoisture);
//    soil = (String)soilint;

//    String soilStr = "[soilhum="+soil+"]";
//    Serial3.println(soilStr);
//    Serial.println(soilStr);
//    Serial.println(ADC_SoilMoisture);

  }
  //============================= Send Data to ESP =======================
  if (CurrentTime >= PreviousTime_send + interval_send and checkhotspot == 0) {
  //send to nodered

  //make string
    String msgtoesp = "";
    if (checksensor == true) {
      msgtoesp += "{\"check\":\"W\",";
    } else {
      msgtoesp += "{\"check\":\"NW\",";
    }
    msgtoesp += "\"id\":\"";
    msgtoesp += String(mac);
    msgtoesp += "\",\"temp\":";
    String temp;
    for (int i = 0; i < String(float(temperature.mean())).length() - 1; i++) {
      temp += String(float(temperature.mean()))[i];
    }
    msgtoesp += temp;
    msgtoesp += ",\"humid\":";
    msgtoesp += String(int(humidity.mean()));
    msgtoesp += ",\"soilh\":";
    msgtoesp += String(int(soilhumidity.mean()));
    msgtoesp += ",\"r\":";
    msgtoesp += "[";
    msgtoesp += ssr1_status;
    msgtoesp += ",";
    msgtoesp += ssr2_status;
    msgtoesp += ",";
    msgtoesp += Relay1_status;
    msgtoesp += ",";
    msgtoesp += Relay2_status;
    msgtoesp += ",";
    msgtoesp += Relay3_status;
    msgtoesp += "]";
    msgtoesp += "}";
//    Serial.print ("Free memory is: ");
//    Serial.println (freeMemory ());
    Serial.println("SEND to ESP!");
    Serial.println(msgtoesp);
    Serial3.println(msgtoesp);
    PreviousTime_send = CurrentTime;
  }
//  Serial.print("worktime :");
//  Serial.println(millis()-checkworktime);
  wdt_reset();
}


void serialEvent2() // Read Data from RS485 ModBus RTU
{
  while (Serial2.available())
  {
//    Serial.println("Serial2 loop");
    byte respone = Serial2.read();
    if (count < 3) {
      if (respone == ADC_check[count]) {
        ADC_Soil[count] = respone;
        count++;
      }
      else
        count = 0;
    }
    else {
      ADC_Soil[count] = respone;
      count++;
    }
//    Serial.print("count:");
//    Serial.print(count);
//    Serial.print(" ");
//    Serial.println(respone,HEX);
  }
  if (count > 8)
  {
    ReceiveedData = true;
    Serial2.end();
    Serial2.flush();
  }
}

void updaterelaystatus() {
  Relay1_status = digitalRead(Relay1);
  Relay2_status = digitalRead(Relay2);
  Relay3_status = digitalRead(Relay3);
  ssr1_status   = digitalRead(SSR1);
  ssr2_status   = digitalRead(SSR2);
  String msgtoesp = "";
  msgtoesp += "{\"id\":\"";
  msgtoesp += String(mac);
  msgtoesp += "\",\"temp\":";
  String temp;
  for (int i = 0; i < String(float(temperature.mean())).length() - 1; i++) {
    temp += String(float(temperature.mean()))[i];
  }
  msgtoesp += temp;
  msgtoesp += ",\"humid\":";
  msgtoesp += String(int(humidity.mean()));
  msgtoesp += ",\"soilh\":";
  msgtoesp += String(int(soilhumidity.mean()));
  msgtoesp += ",\"r\":";
  msgtoesp += "[";
  msgtoesp += ssr1_status;
  msgtoesp += ",";
  msgtoesp += ssr2_status;
  msgtoesp += ",";
  msgtoesp += Relay1_status;
  msgtoesp += ",";
  msgtoesp += Relay2_status;
  msgtoesp += ",";
  msgtoesp += Relay3_status;
  msgtoesp += "]";
  msgtoesp += "}";
  Serial.print ("Free memory is: ");
  Serial.println (freeMemory ());
  Serial.println("Update relay status!");
  Serial.println(msgtoesp);
  Serial3.println(msgtoesp);
}
void serialEvent3()
{
  while (Serial3.available())
  {
//    Serial.println("Serial3 loop");
    String inByte = Serial3.readString();
//-------------------------------------  แก้อ่าน string ไม่ครบ ????
//      delay(1000);
    Serial.println(inByte);
    if (inByte.indexOf("byte") != -1) {
      //update Mega
      showoled_update();
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(inByte);
      const char* bytesize = root["byte"];
      long bytesizenum = atol(bytesize);
      Serial.print("Get size ");
      Serial.println(bytesizenum);
      handleSketchDownload(bytesizenum);
    }
    if (inByte.indexOf("nwf:") != -1) {
      wifiname = "";
      int checklength = inByte.length() - 7;
      if (checklength >= 12) {
        checklength = 10;
      }
      int startindex = inByte.lastIndexOf("nwf:") + 4;
      for (int i = 0; i <= checklength; i++) {
        if (i >= 10) {
          break;
        }
        if (inByte[i + startindex] != "\n") {
          wifiname += char(inByte[i + startindex]);
        }
      }
      Serial.print("newwifi:");
      Serial.println(wifiname);
    }
    if (inByte.indexOf("macaddress") != -1 and mac.length() != 12) {
      for (int i = 0; i <= 11; i++) {
        mac += char(inByte[i + 14]);
      }
//        Serial.println("wifinameindex");
//        Serial.println(inByte.lastIndexOf("wifiname:"));
//        Serial.println(inByte.lastIndexOf("$"));
      int checklength = inByte.indexOf("$") - inByte.lastIndexOf("wifiname:") - 10;
      int startindex = inByte.lastIndexOf("wifiname:") + 9;
      for (int i = 0; i <= checklength; i++) {
        wifiname += char(inByte[i + startindex]);
      }
      Serial.println("wifi snap:");
      Serial.println(wifiname);
      Serial.println("----------");
      Serial.println("mac snap:");
      Serial.println(mac);
      Serial.println("----------");
    }
    if (inByte.indexOf("Connected") != -1) {
      Serial.println("mega:connected");
      greenled();
      if (checkhotspot == 1) {
        wifiname = "";
        //get new wifi name
        delay(2500);
        Serial3.println("nwf");
        checkhotspot = 0;
        delay(1000);
      }
      showoled();
      checklosswifi = 0;
    }
    if (inByte.indexOf("HTTP") != -1) {
      Serial.println("mega:Hospot");
      showoled_hospot();
      checkhotspot = 1;
      redled();
    }
    if (inByte.indexOf("Loss wifi") != -1) {
      Serial.println("mega:Loss wifi");
      checklosswifi = 1;
      redled();
    }
    if (inByte.indexOf("message arrived") != -1) {
      Serial.println("mega:internet ok");
      greenled();
    }
    //@$K=internetOK
    if (inByte.indexOf("@$K") != -1) {
      Serial.println("mega:internet ok");
      greenled();
    }
    //@$N=internetNOTOK
    if (inByte.indexOf("@$N") != -1) {
      Serial.println("mega:internet not ok");
      blueled();
    }
    if (inByte.indexOf("1~") != -1) {
      digitalWrite(SSR1, HIGH);
    }
    if (inByte.indexOf("1`") != -1) {
      digitalWrite(SSR1, LOW);
    }
    if (inByte.indexOf("2~") != -1) {
      digitalWrite(SSR2, HIGH);
    }
    if (inByte.indexOf("2`") != -1) {
      digitalWrite(SSR2, LOW);
    }
    if (inByte.indexOf("3~") != -1) {
      digitalWrite(Relay1, HIGH);
    }
    if (inByte.indexOf("3`") != -1) {
      digitalWrite(Relay1, LOW);
    }
    if (inByte.indexOf("4~") != -1) {
      digitalWrite(Relay2, HIGH);
    }
    if (inByte.indexOf("4`") != -1) {
      digitalWrite(Relay2, LOW);
    }
    if (inByte.indexOf("5~") != -1) {
      digitalWrite(Relay3, HIGH);
      updaterelaystatus();
      showoled();
//      break;
    }
    if (inByte.indexOf("5`") != -1) {
      digitalWrite(Relay3, LOW);
      updaterelaystatus();
      showoled();
//      break;
    }
  }
}
