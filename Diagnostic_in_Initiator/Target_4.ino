#include <Wire.h>
#include <Adafruit_seesaw.h>
#include <Stepper.h>

#define SENSOR_SDA 21
#define SENSOR_SCL 22
#define I2CBUS2_SDA 18
#define I2CBUS2_SCL 19
// ---------- CONFIG ----------
#define SLAVE_ADDR       0x45
#define SAMPLES          10

// ---------- COMMANDS ----------
#define CMD_START        0x01
#define CMD_STATUS       0x02
#define CMD_DATA         0x03

// ---------- STATUS ----------
#define ST_IDLE          0x00
#define ST_COLLECTING    0x01
#define ST_DONE          0x02
int32_t arr[SAMPLES]={0};
int32_t lastPos = 0;

// ---------- SENSOR ----------
TwoWire I2C_BUS2=TwoWire(1);
Adafruit_seesaw encoder;

// Stepper Motor (28BYJ-48 with ULN2003 driver)
const int stepsPerRevolution = 2048; 
Stepper stepper(stepsPerRevolution, 25, 26, 27, 14); 

// ---------- GLOBAL STATE ----------
volatile uint8_t lastCmd = 0;
volatile uint8_t reqIndex = 0;
volatile bool startRequested = false;

uint8_t status = ST_IDLE;

// -------- I2C RECEIVE --------
void onReceive(int len) {
  if (!I2C_BUS2.available()) return;

  lastCmd = I2C_BUS2.read();

  if (lastCmd == CMD_START) {
    startRequested = true;
    reqIndex   = 0;
  }
  else if (lastCmd == CMD_DATA&& I2C_BUS2.available()) {
    reqIndex  = I2C_BUS2.read();
  }
}
void onRequest() {

  if (lastCmd == CMD_STATUS) {
    I2C_BUS2.write(status);
  }
  else if (lastCmd == CMD_DATA) {
      int32_t encoder = arr[reqIndex];
       I2C_BUS2.write((uint8_t*)&encoder, sizeof(int32_t));
  }
}

// =================================================
// MONITORING
void Monitoring() {
  memset(arr, 0, sizeof(arr));
  for (int i=0;i<SAMPLES;) {
    stepper.step(85); // ~250 ms @ 10 RPM
    int32_t newPos = encoder.getEncoderPosition();
    if((newPos>=100000)||(newPos<=-100000)||(newPos==0))
    {
      arr[i++] = newPos;
    }
    else if(newPos!=lastPos)
    {
       arr[i++] = newPos;
      lastPos=newPos;
    }
  }
}

void setup() {
    Serial.begin(115200);
// MAIN I2C BUS → Sensors
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  if (!encoder.begin(0X36)) {
    Serial.println(" No ENCODER found!");
  }
  else
  {
    Serial.println("ENCODER found");
  }
   I2C_BUS2.begin(SLAVE_ADDR,I2CBUS2_SDA,I2CBUS2_SCL,100000);
   I2C_BUS2.onReceive(onReceive);
   I2C_BUS2.onRequest(onRequest);
  // Initialize stepper
  stepper.setSpeed(10); // RPM
  stepper.step(85);
  int32_t newPos = encoder.getEncoderPosition();
  encoder.setEncoderPosition(0);

}
void loop() {
  if (startRequested) {
    startRequested = false;
    status = ST_IDLE;
    reqIndex = 0;

    status = ST_COLLECTING;

    Monitoring();

    status = ST_DONE;
  }
}
