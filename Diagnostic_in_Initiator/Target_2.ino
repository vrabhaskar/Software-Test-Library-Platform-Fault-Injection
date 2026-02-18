
#include <Wire.h>
#include <Adafruit_TSL2591.h>


#define SENSOR_SDA 21
#define SENSOR_SCL 22
#define I2CBUS2_SDA 18
#define I2CBUS2_SCL 19
#define GPIO23 23
// -------- SLAVE CONFIG --------
#define SLAVE_ADDR 0x43
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
TwoWire I2C_BUS2= TwoWire(1);
Adafruit_TSL2591 tsl(2591);

// -------- DATA --------
float samples[SAMPLES];
volatile uint8_t reqIndex = 0;
float data =0;

// -------- I2C RECEIVE --------
void onReceive(int len) {
  if (!I2C_BUS2.available()) return;

  lastCmd = I2C_BUS2.read();

  if (lastCmd == CMD_START) {
    status = ST_COLLECTING;
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
    data = samples[reqIndex];
       I2C_BUS2.write((uint8_t*)&data, sizeof(float));
      }
}

// -------- MONITORING --------
void Monitoring() {
  sensors_event_t event;
  for(int i=0;i<SAMPLES;i++)
  {
     tsl.getEvent(&event);
     samples[i] = event.light;
     vTaskDelay(250 / portTICK_PERIOD_MS);
     Serial.println(samples[i]);
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
   Wire.begin(SENSOR_SDA, SENSOR_SCL);
  if (!tsl.begin(0x29)) {
    Serial.println("tsl2591 not found");
  }else 
  Serial.println("tsl2591 found");

  tsl.setGain(TSL2591_GAIN_MED);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
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
    Serial.println("TARGET 2  is start collecting");
    status = ST_COLLECTING;
    digitalWrite(GPIO23, LOW);
    Monitoring();
    digitalWrite(GPIO23, HIGH);
    status = ST_DONE;
  }
}
