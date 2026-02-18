#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Update.h>

TwoWire I2CBUS = TwoWire(1);

#define SDA_PIN 18
#define SCL_PIN 19
// -------- SLAVE ADDRESSES --------
/* 0x42 -Targetesp32_1 slave   
   0x43-Targetesp32_2 slave
   0x44-Targetesp32_3 Slave
   0x45-ROTARY Targetesp32_4 Slave*/

int32_t Target_addr[4]={0x42,0x43,0x44,0x45};
#define SAMPLES          11
// -------- COMMANDS --------
#define CMD_START       0x01
#define CMD_STATUS      0x02
#define CMD_ERROR_COUNT 0x03
#define CMD_ERROR_VALUE 0x04

// -------- STATUS VALUES --------
#define ST_IDLE        0x00
#define ST_COLLECTING  0x01
#define ST_DONE        0x02



#define WIFI_SSID     "JioFiber-J9Jmq"
#define WIFI_PASSWORD "IateiGa5Eifieph6"


String currentVersion = "1.0.0";
String versionURL = "https://vrabhaskar.github.io/esp32_ota/version.txt";
String binURL     = "https://vrabhaskar.github.io/esp32_ota/firmware.bin";

int32_t Control_pins[4]={25,26,27,14};




void performGitHubOTA(const String& vURL, const String& bURL, const String& curVer) {
  HTTPClient http;
  Serial.println(" Checking for OTA...");
  if (!http.begin(vURL)) return;
  int code = http.GET();
  if (code == 200) {
    String newVer = http.getString(); newVer.trim();
    Serial.printf("Current:%s  Remote:%s\n", curVer.c_str(), newVer.c_str());
    if (newVer != curVer) {
      http.end();
      HTTPClient hb; WiFiClient wcli;
      if (!hb.begin(wcli, bURL)) return;
      int bcode = hb.GET();
      if (bcode == 200) {
        int len = hb.getSize(); WiFiClient* s = hb.getStreamPtr();
        if (!Update.begin(len)) { hb.end(); return; }
        size_t w = Update.writeStream(*s);
        bool ok = Update.end(true); hb.end();
        if (ok && w == (size_t)len) { Serial.println(" OTA done, rebooting..."); delay(500); ESP.restart(); }
      }
    } else Serial.println(" Already latest.");
  }
  http.end();
}

uint8_t readStatus(uint8_t addr) {

  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_STATUS);

  if (I2CBUS.endTransmission(false) != 0) {
    return 0xFF;   // communication error
  }

  if (I2CBUS.requestFrom(addr, (uint8_t)1) != 1) {
    return 0xFF;
  }

  return I2CBUS.read();
}


uint8_t readErrorCount(uint8_t addr) {
  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_ERROR_COUNT);
  I2CBUS.endTransmission();
  I2CBUS.requestFrom(addr, (uint8_t)1);
  if (I2CBUS.available()) {
    
   uint8_t count=I2CBUS.read();
   return count;
  }
return 0;
}

void printSlaveErrors(const char* name, uint8_t addr) {
  uint8_t count=0;
  uint8_t count1 = readErrorCount(addr);
  uint8_t count2 = readErrorCount(addr);
  if (count1 != count2){
    count = readErrorCount(addr);
  } 
  else
  {
    count=count1;
  }

  Serial.print(name);

  if (count < 3) {
    Serial.println("  PASS");
  }
  else
  {
   Serial.println("  FAIL"); 
  }
}

void Normal_operation()
{
  for(int i=0;i<4;i++)
  {
  digitalWrite(Control_pins[i], HIGH);
  }
}
void Fault_inject()
{
  for(int i=0;i<4;i++)
  {
    digitalWrite(Control_pins[i], LOW);
  }
}

void command_to_begintransmission()
{
  for(int i=0;i<4;i++)
  {
    I2CBUS.beginTransmission(Target_addr[i]);
    I2CBUS.write(CMD_START);
    I2CBUS.endTransmission();

  }
  Serial.println("START sent to all slaves");
}
void collecting_status() 
{
  unsigned long t0 = millis();
  Serial.print("Collecting ");

  while (millis() - t0 < 30000)
  {
    bool allDone = true;

    for(int i=0;i<4;i++)
    {
      uint8_t st = readStatus(Target_addr[i]);

      if (st != ST_DONE) {
        allDone = false;
      }
    }

    if(allDone) {
      Serial.println(" DONE");
      return;
    }

    Serial.print(".");
    delay(50);
  }

  for(int i=0;i<4;i++)
  {
    uint8_t st = readStatus(Target_addr[i]);

    if (st != ST_DONE)
    {
      Serial.printf("TARGET %d (0x%X) NOT finished\n",
                    i+1, Target_addr[i]);
    }
  }
}


void startCommunication()
{
  Serial.println("\n Starting normal operation...");

  Normal_operation();
  command_to_begintransmission();
  delay(500);
  collecting_status();
  printSlaveErrors("MCP9808  =", Target_addr[0]);
  printSlaveErrors("TSL2591  =", Target_addr[1]);
  printSlaveErrors("TCS34725 =", Target_addr[2]);
  printSlaveErrors("ENOCDER  =", Target_addr[3]);

  Serial.println("\n Starting fault injection...");

  Fault_inject();
  command_to_begintransmission();
  delay(500);
  collecting_status();
  printSlaveErrors("MCP9808=", Target_addr[0]);
  printSlaveErrors("TSL2591=", Target_addr[1]);
  printSlaveErrors("TCS34725=", Target_addr[2]);
  printSlaveErrors("ENOCDER=", Target_addr[3]);


  Normal_operation();

  Serial.println("\n Cycle finished");
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  performGitHubOTA(versionURL, binURL, currentVersion);
  I2CBUS.begin(SDA_PIN, SCL_PIN, 100000); // 2nd bus
  for(int i=0;i<4;i++)
  {
    pinMode(Control_pins[i], OUTPUT);
    digitalWrite(Control_pins[i], HIGH);
    delay(10);
  }
  Normal_operation();
   
  Serial.println("Initiator READY");
  
}


void loop() {
   //enter START in serial monitor, it will start the startcommunication()
  if (Serial.available())
  {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    cmd.toUpperCase();

    if (cmd == "START")
    {
      startCommunication();
    }
}
}

