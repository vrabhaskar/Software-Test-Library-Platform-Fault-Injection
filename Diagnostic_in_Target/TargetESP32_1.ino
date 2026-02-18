
#include <Wire.h>
#include <Adafruit_TSL2591.h>


#define SENSOR_SDA 21
#define SENSOR_SCL 22
#define I2CBUS2_SDA 18
#define I2CBUS2_SCL 19
#define GPIO23 23

#define SLAVE_ADDR 0x43
#define SAMPLES 10


#define CMD_START       0x01
#define CMD_STATUS      0x02
#define CMD_ERROR_COUNT 0x03

// -------- STATUS --------
#define ST_IDLE        0x00
#define ST_COLLECTING  0x01
#define ST_DONE        0x02

volatile uint8_t status = ST_IDLE;
volatile uint8_t lastCmd = 0;
volatile bool startRequested = false;


TwoWire I2C_BUS2 = TwoWire(1);
Adafruit_TSL2591 tsl(2591);


float samples[SAMPLES];
uint8_t errorCount = 0;

void onReceive(int len) {
  if (!I2C_BUS2.available()) return;

  lastCmd = I2C_BUS2.read();

  if (lastCmd == CMD_START) {
    status = ST_COLLECTING;
    startRequested = true;
    errorCount = 0;
  }
}

void onRequest() {

  if (lastCmd == CMD_STATUS) {
    I2C_BUS2.write(status);
  }
  else if (lastCmd == CMD_ERROR_COUNT) {
    I2C_BUS2.write(errorCount);
  }
}

void Diagnostics() {
  errorCount = 0;

  for (int i = 0; i < SAMPLES; i++) {
    float lux = samples[i];
    if(isnan(lux))
    {
      errorCount++;
    }
    else if (lux <= 0.5 || lux > 88000) {
        errorCount++;
      }
    }
}

void Monitoring() {
  sensors_event_t event;
  for(int i=0;i<SAMPLES;i++)
  {
     tsl.getEvent(&event);
     samples[i] = event.light;
     vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

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

void loop() {
 if (startRequested) {
    startRequested = false;
    status = ST_COLLECTING;
    digitalWrite(GPIO23, LOW);
    Monitoring();
    Diagnostics();
    digitalWrite(GPIO23, HIGH);
    status = ST_DONE;
  }
}


