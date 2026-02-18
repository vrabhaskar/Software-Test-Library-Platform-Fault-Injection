#include <Wire.h>
#include <Adafruit_MCP9808.h>

#define SENSOR_SDA 21
#define SENSOR_SCL 22
#define I2CBUS2_SDA 18
#define I2CBUS2_SCL 19
#define GPIO23 23
// -------- SLAVE CONFIG --------
#define SLAVE_ADDR 0x42
#define SAMPLES 10

// -------- COMMANDS --------
#define CMD_START       0x01
#define CMD_STATUS      0x02
#define CMD_DATA        0x03

// -------- STATUS --------
#define ST_IDLE        0x00
#define ST_COLLECTING  0x01
#define ST_DONE        0x02

volatile uint8_t status = ST_IDLE;
volatile uint8_t lastCmd = 0;
volatile bool startRequested = false;

// -------- SENSOR --------
TwoWire I2C_BUS2 = TwoWire(1);
Adafruit_MCP9808 mcp;

// -------- DATA --------
float samples[SAMPLES];
volatile uint8_t reqIndex = 0;


// -------- I2C RECEIVE --------
void onReceive(int len) {
  if (!I2C_BUS2.available()) return;

  lastCmd = I2C_BUS2.read();

  if (lastCmd == CMD_START) {
    status = ST_IDLE;
    startRequested = true;
    reqIndex   = 0;
  }
  else if (lastCmd == CMD_DATA&& I2C_BUS2.available()) {
    reqIndex  = I2C_BUS2.read();
  }
}

// -------- I2C REQUEST --------
void onRequest() {

  if (lastCmd == CMD_STATUS) {
    I2C_BUS2.write(status);
  }
  else if (lastCmd == CMD_DATA) {
    if (reqIndex <SAMPLES) {
      float temp = samples[reqIndex];
       I2C_BUS2.write((uint8_t*)&temp, sizeof(float));
    } 
  }
}

// -------- MONITORING --------
void Monitoring() {
  for(int i=0;i<SAMPLES;i++)
  {
    vTaskDelay(250 / portTICK_PERIOD_MS);
    samples[i] = mcp.readTempC();
    Serial.println(samples[i]);
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
// MAIN I2C BUS → Sensors
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  if (!mcp.begin(0x1F)) {
     Serial.println("MCP9808 not found");
  }else
  Serial.println("Mcp9808 found");
   // Optional: Set resolution (0 to 3)
  mcp.setResolution(0);  // 0 = 0.5°C (fastest)
   I2C_BUS2.begin(SLAVE_ADDR,I2CBUS2_SDA,I2CBUS2_SCL,100000);
   I2C_BUS2.onReceive(onReceive);
   I2C_BUS2.onRequest(onRequest);
   pinMode(GPIO23,OUTPUT);
   digitalWrite(GPIO23, HIGH);
}

// -------- LOOP --------
void loop() {
  if (startRequested) {
    startRequested = false;

    status = ST_IDLE;
    reqIndex = 0;
    Serial.println("TARGET 1  is start collecting");
    status = ST_COLLECTING;
  digitalWrite(GPIO23, LOW);
    Monitoring();
    status = ST_DONE;
    digitalWrite(GPIO23, HIGH);
  }
}

