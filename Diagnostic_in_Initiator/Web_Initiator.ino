#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Update.h>

TwoWire I2CBUS = TwoWire(1);

// ---------------- CONFIG ----------------
#define SDA_PIN 18
#define SCL_PIN 19
#define SAMPLES 11

#define CMD_START  0x01
#define CMD_STATUS 0x02
#define CMD_DATA   0x03
#define ST_DONE    0x02

#define WIFI_SSID     "JioFiber-J9Jmq"
#define WIFI_PASSWORD "IateiGa5Eifieph6"


String currentVersion = "1.0.0";
String versionURL = "https://vrabhaskar.github.io/esp32_ota/version.txt";
String binURL     = "https://vrabhaskar.github.io/esp32_ota/firmware.bin";

WebServer server(80);

int32_t Target_addr[4] = {0x42,0x43,0x44,0x45};
int32_t Control_pins[4] = {25,26,27,14};

float   mcp[SAMPLES];
float   tsl[SAMPLES];
int32_t tcs[SAMPLES];
int32_t encoder[SAMPLES];

uint8_t expectedColorID[10] = {1,2,3,1,2,3,1,2,3,2};

uint32_t ErrorCount1,ErrorCount2,ErrorCount3,ErrorCount4;

String liveLog = "";
bool testRunning = false;

// ---------------- WEB PAGE ----------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Dashboard</title>
<style>
body{font-family:Arial;text-align:center;margin-top:40px}
button{width:220px;height:65px;font-size:22px;background:#2ecc71;color:white;border:none;border-radius:10px}
pre{margin-top:20px;text-align:left;background:#eee;padding:15px;white-space:pre-wrap;height:400px;overflow:auto}
</style>
</head>
<body>

<h2>ESP32 Initiator </h2>

<button onclick="start()">START TEST</button>

<pre id="out">Press START...</pre>

<script>
let interval;

function start(){
  fetch('/api/start');
  document.getElementById("out").innerText="Starting...\n";
  interval = setInterval(updateLog,500);
}

function updateLog(){
  fetch('/api/log')
  .then(r=>r.text())
  .then(d=>{
    document.getElementById("out").innerText=d;
  });
}
</script>

</body>
</html>
)rawliteral";




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
// ---------------- I2C ----------------
uint8_t readStatus(uint8_t addr)
{
  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_STATUS);
  I2CBUS.endTransmission();

  I2CBUS.requestFrom(addr,(uint8_t)1);
  if(I2CBUS.available())
    return I2CBUS.read();

  return 0xFF;
}

void Normal_operation()
{
  for(int i=0;i<4;i++)
    digitalWrite(Control_pins[i], HIGH);
}

void Fault_inject()
{
  for(int i=0;i<4;i++)
    digitalWrite(Control_pins[i], LOW);
}

void command_to_begintransmission()
{
  for(int i=0;i<4;i++)
  {
    I2CBUS.beginTransmission(Target_addr[i]);
    I2CBUS.write(CMD_START);
    I2CBUS.endTransmission();
  }
}

void collecting_status()
{
  liveLog += "Collecting status ";
  bool done[4]={0};
  unsigned long t0 = millis();

  while(millis()-t0 < 10000)
  {
    bool allDone=true;

    for(int i=0;i<4;i++)
    {
      if(readStatus(Target_addr[i])==ST_DONE)
        done[i]=true;

      if(!done[i]) allDone=false;
    }

    if(allDone) break;

    liveLog += ".";
    delay(50);
  }
  liveLog += "\n";
}

void i2c_read_data(uint8_t addr,uint8_t index,void* buf,uint8_t len)
{
  memset(buf,0,len);

  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_DATA);
  I2CBUS.write(index);
  if(I2CBUS.endTransmission(false)!=0)
    return;

  I2CBUS.requestFrom(addr,len,true);
  if(I2CBUS.available()==len)
    I2CBUS.readBytes((uint8_t*)buf,len);
}

void collect_samples()
{
  liveLog += "Collecting samples...\n";

  for(int i=0;i<SAMPLES;i++)
  {
    i2c_read_data(Target_addr[0],i,&mcp[i],sizeof(float));
    i2c_read_data(Target_addr[1],i,&tsl[i],sizeof(float));
    i2c_read_data(Target_addr[2],i,&tcs[i],sizeof(int32_t));
    i2c_read_data(Target_addr[3],i,&encoder[i],sizeof(int32_t));
    if(i!=0)
    {
       liveLog += "m=" + String(mcp[i],2);
    liveLog += " Tsl=" + String(tsl[i],2);
    liveLog += " Tcs=" + String(tcs[i]);
    liveLog += " Rotary=" + String(encoder[i]);
    liveLog += "\n";

    delay(200);

    }
   
  }
}

// ---------------- DIAGNOSTICS ----------------
void diagnosticsReport()
{
  liveLog += "\n=== DIAGNOSTICS ===\n";

  ErrorCount2=0;
  for(int i=1;i<SAMPLES;i++)
    if(isnan(mcp[i]) || mcp[i]<-40.0 || mcp[i]>125.0)
      ErrorCount2++;
  liveLog += (ErrorCount2<3) ? "MCP: PASS\n" : "MCP: FAIL\n";

  ErrorCount1=0;
  for(int i=1;i<SAMPLES;i++)
    if(isnan(tsl[i]) || tsl[i]<=0.50 || tsl[i]>88000.0)
      ErrorCount1++;
  liveLog += (ErrorCount1<3) ? "TSL: PASS\n" : "TSL: FAIL\n";

  ErrorCount3=0;
  for(int i=1,j=0;i<SAMPLES;i++,j++)
    if(tcs[i] != expectedColorID[j])
      ErrorCount3++;
  liveLog += (ErrorCount3<3) ? "TCS: PASS\n" : "TCS: FAIL\n";

  ErrorCount4=0;
  for(int i=1;i<SAMPLES-1;i++)
    if(encoder[i]==encoder[i+1] || encoder[i]<-10000 || encoder[i]>10000)
      ErrorCount4++;
  liveLog += (ErrorCount4<3) ? "ENCODER: PASS\n" : "ENCODER: FAIL\n";
}

// ---------------- MAIN TEST ----------------
void startCommunication()
{
  liveLog = "";
  testRunning = true;

  liveLog += "Starting normal operation...\n";
  Normal_operation();

  liveLog += "Sending START to slaves...\n";
  command_to_begintransmission();

  collecting_status();
  collect_samples();
  diagnosticsReport();

  liveLog += "\nStarting fault injection...\n";
  Fault_inject();

  command_to_begintransmission();
  collecting_status();
  collect_samples();
  diagnosticsReport();

  Normal_operation();

  liveLog += "\nCycle finished.\n";

  testRunning = false;
}

// Background task
void startTask(void *parameter)
{
  startCommunication();
  vTaskDelete(NULL);
}

// ---------------- WEB API ----------------
void apiStart()
{
  if(!testRunning)
    xTaskCreate(startTask,"TestTask",10000,NULL,1,NULL);

  server.send(200,"text/plain","Started");
}

void apiLog()
{
  server.send(200,"text/plain",liveLog);
}

void handleRoot()
{
  server.send(200,"text/html",webpage);
}

// ---------------- SETUP ----------------
void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  while(WiFi.status()!=WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("Open browser: http://");
  Serial.println(WiFi.localIP());

  I2CBUS.begin(SDA_PIN,SCL_PIN,100000);

  for(int i=0;i<4;i++)
  {
    pinMode(Control_pins[i],OUTPUT);
    digitalWrite(Control_pins[i],HIGH);
  }
  performGitHubOTA(versionURL, binURL, currentVersion);
  server.on("/",handleRoot);
  server.on("/api/start",apiStart);
  server.on("/api/log",apiLog);
  server.begin();
}

void loop()
{
  server.handleClient();
}
