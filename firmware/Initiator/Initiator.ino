#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Update.h>

TwoWire I2CBUS = TwoWire(1);
// -------- SLAVE ADDRESSES --------
/* 0x42 -Targetesp32_1 slave   
   0x43-Targetesp32_2 slave
   0x44-Targetesp32_3 Slave
   0x45-ROTARY Targetesp32_4 Slave*/

int32_t Target_addr[4]={0x42,0x43,0x44,0x45};
#define SAMPLES          11

#define SDA_PIN 18
#define SCL_PIN 19
// -------- COMMANDS --------
#define CMD_START       0x01
#define CMD_STATUS      0x02
#define CMD_DATA        0x03

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

float mcp[SAMPLES]={0};
float tsl[SAMPLES]={0};
int32_t tcs[SAMPLES]={0};
int32_t encoder[SAMPLES]={0};

uint32_t ErrorCount1 = 0;
uint32_t ErrorCount2 = 0;
uint32_t ErrorCount3 = 0;
uint32_t ErrorCount4 = 0;


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

// ------------------------------------------------
// READ SLAVE STATUS
// ------------------------------------------------
uint8_t readStatus(uint8_t addr) {
  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_STATUS);
  I2CBUS.endTransmission();

  I2CBUS.requestFrom(addr, (uint8_t)1);
  if (I2CBUS.available()) {
    return I2CBUS.read();
  }
  return 0xFF;   // error
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
  bool done[4] = {0};
  unsigned long t0 = millis();

  Serial.print("Collecting ");

  while (millis() - t0 < 20000)
  {
    bool allDone = true;

    for(int i=0;i<4;i++)
    {
      if(readStatus(Target_addr[i]) == ST_DONE)
        done[i] = true;

      if(!done[i]) allDone = false;
    }

    if(allDone) break;

    Serial.print(".");
    delay(50);
  }

  Serial.println();

  for(int i=0;i<4;i++)
    if(!done[i])
      Serial.printf("TARGET %d (0x%X) NOT done\n", i+1, Target_addr[i]);
}

void collect_samples(void)
{
    for(int i=0;i<SAMPLES;i++)
    {
        i2c_read_data(0x42,i,&mcp[i],sizeof(float));
        i2c_read_data(0x43,i,&tsl[i],sizeof(float));
        i2c_read_data(0x44,i,&tcs[i],sizeof(int32_t));
        i2c_read_data(0x45,i,&encoder[i],sizeof(int32_t));
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void i2c_read_data(uint8_t addr, uint8_t index, void* buf, uint8_t len)
{
  memset(buf, 0, len);
    I2CBUS.beginTransmission(addr);
    I2CBUS.write(CMD_DATA);
    I2CBUS.write(index);
    if(I2CBUS.endTransmission(true) != 0)
  {
    Serial.println("TX fail");
    return;
  }
vTaskDelay(250 / portTICK_PERIOD_MS);
while(I2CBUS.available())
{
  I2CBUS.read();
}
    I2CBUS.requestFrom(addr,len);

    if (I2CBUS.available() == len)
        I2CBUS.readBytes((uint8_t*)buf, len);
}


uint8_t expectedColorID[10] = {1,2,3,1,2,3,1,2,3,2};



void tsl2591_diagnostics()
{
  ErrorCount1=0;
  for(int i=1;i<SAMPLES;i++)
  {
      if(isnan(tsl[i]))
      {
        ErrorCount1++;
      }
      else if(tsl[i]<=0.50 || tsl[i]> 88000.00)
      {
        ErrorCount1++;
      }
  }
if(ErrorCount1<3)
{
Serial.println("TSL:PASS");
}
else 
{
Serial.println("TSL:FAIL");
}
}



void mcp9808_diagnostics()
{
  ErrorCount2=0;
  for(int i=1;i<SAMPLES;i++)
  {
      if(isnan(mcp[i]))
      {
        ErrorCount2++;
      }
      else if(mcp[i]< -40.0 || mcp[i]> 125.0)
      {
       ErrorCount2++;
      }
  }
if(ErrorCount2<3)
{
Serial.println("MCP:PASS");
}
else 
{
Serial.println("MCP:FAIL");
}


}

void tcs34725_diagnostics()
{
  ErrorCount3=0;
  for(int i=1,j=0;i<SAMPLES;i++,j++)
  {
    if(tcs[i] != expectedColorID[j]) {
        ErrorCount3++;
      }
  }
if(ErrorCount3<3)
{
Serial.println("TCS:PASS");
}
else 
{
Serial.println("TCS:FAIL");
}
}



void rotaryecnoder_diagnostics()
{
  ErrorCount4=0;
  for(int i=1;i<SAMPLES;i++)
  {
    if(encoder[i]==encoder[i+1])
    {
      ErrorCount4++;
    }
    else if (encoder[i]<-10000 ||encoder[i]>10000 ) {
       ErrorCount4++;
      }
  }
if(ErrorCount4<3)
{
Serial.println("ENCODER:PASS");
}
else 
{
Serial.println("ENCODER:FAIL or NO MOEMENT IN ENCODER");
}
}

void Print_data()
{
  for(int32_t i=1;i<SAMPLES;i++)
  {
    Serial.print("m=");
    Serial.print(mcp[i]);
    Serial.print(" Tsl=");
    Serial.print(tsl[i]);
    Serial.print(" Tcs=");
    Serial.print(tcs[i]);
    Serial.print(" rotary=");
    Serial.println(encoder[i]);
  }
}
void Diagnostics_checks()
{
  mcp9808_diagnostics();
  tsl2591_diagnostics();
  tcs34725_diagnostics();
  rotaryecnoder_diagnostics();
}


void startCommunication()
{
  Serial.println("\n Starting normal operation...");

  Normal_operation();
  command_to_begintransmission();
  collecting_status();
  collect_samples();
  Print_data();
  Diagnostics_checks();

  Serial.println("\n Starting fault injection...");

  Fault_inject();
  command_to_begintransmission();
  collecting_status();
  collect_samples();
  Print_data();
  Diagnostics_checks();

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
