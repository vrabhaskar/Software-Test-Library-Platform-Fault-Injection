#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Update.h>

TwoWire I2CBUS = TwoWire(1);
WebServer server(80);

#define SDA_PIN 18
#define SCL_PIN 19

#define CMD_START       0x01
#define CMD_STATUS      0x02
#define CMD_ERROR_COUNT 0x03
#define ST_DONE         0x02

#define WIFI_SSID     "JioFiber-J9Jmq"
#define WIFI_PASSWORD "IateiGa5Eifieph6"

String currentVersion = "1.0.0";
String versionURL = "https://vrabhaskar.github.io/esp32_ota/version.txt";
String binURL     = "https://vrabhaskar.github.io/esp32_ota/firmware.bin";

int32_t Target_addr[4] = {0x42,0x43,0x44,0x45};
int32_t Control_pins[4] = {25,26,27,14};

String liveLog = "";
bool testRunning = false;

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

<h2>ESP32 Initiator</h2>
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
  if (!http.begin(vURL)) return;
  int code = http.GET();
  if (code == 200) {
    String newVer = http.getString(); newVer.trim();
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
        if (ok && w == (size_t)len) ESP.restart();
      }
    }
  }
  http.end();
}


uint8_t readStatus(uint8_t addr)
{
  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_STATUS);
  if (I2CBUS.endTransmission(false) != 0) return 0xFF;
  if (I2CBUS.requestFrom(addr,(uint8_t)1) != 1) return 0xFF;
  return I2CBUS.read();
}

uint8_t readErrorCount(uint8_t addr)
{
  I2CBUS.beginTransmission(addr);
  I2CBUS.write(CMD_ERROR_COUNT);
  I2CBUS.endTransmission();
  I2CBUS.requestFrom(addr,(uint8_t)1);
  if(I2CBUS.available())
    return I2CBUS.read();
  return 0;
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
  liveLog += "START sent to all slaves\n";
}

void collecting_status()
{
  unsigned long t0 = millis();
  liveLog += "Collecting ";

  while(millis()-t0 < 30000)
  {
    bool allDone=true;

    for(int i=0;i<4;i++)
    {
      uint8_t st = readStatus(Target_addr[i]);
      if(st != ST_DONE)
        allDone=false;
    }

    if(allDone){
      liveLog += " DONE\n";
      return;
    }

    liveLog += ".";
    delay(50);
  }

  liveLog += "\nTimeout!\n";
}

void printSlaveErrors(const char* name, uint8_t addr)
{
  uint8_t count1 = readErrorCount(addr);
  uint8_t count2 = readErrorCount(addr);
  uint8_t count = (count1==count2) ? count1 : readErrorCount(addr);

  liveLog += name;

  if(count < 3)
    liveLog += " PASS\n";
  else
    liveLog += " FAIL\n";
}

void startCommunication()
{
  liveLog = "";
  testRunning = true;

  liveLog += "Starting normal operation...\n";

  Normal_operation();
  command_to_begintransmission();
  delay(500);
  collecting_status();

  printSlaveErrors("MCP9808  =", Target_addr[0]);
  printSlaveErrors("TSL2591  =", Target_addr[1]);
  printSlaveErrors("TCS34725 =", Target_addr[2]);
  printSlaveErrors("ENCODER  =", Target_addr[3]);

  liveLog += "\nStarting fault injection...\n";

  Fault_inject();
  command_to_begintransmission();
  delay(500);
  collecting_status();

  printSlaveErrors("MCP9808  =", Target_addr[0]);
  printSlaveErrors("TSL2591  =", Target_addr[1]);
  printSlaveErrors("TCS34725 =", Target_addr[2]);
  printSlaveErrors("ENCODER  =", Target_addr[3]);

  Normal_operation();

  liveLog += "\nCycle finished\n";
  testRunning = false;
}

// Run test in background task
void startTask(void *parameter)
{
  startCommunication();
  vTaskDelete(NULL);
}


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


void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  while(WiFi.status()!=WL_CONNECTED)
    delay(300);

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
