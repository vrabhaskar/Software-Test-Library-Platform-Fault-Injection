#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_NeoPixel.h>

#define SENSOR_SDA 21
#define SENSOR_SCL 22
#define I2CBUS2_SDA 18
#define I2CBUS2_SCL 19
// ---------- CONFIG ----------
#define SLAVE_ADDR       0x44
#define SAMPLES          10
#define MAX_ERRORS       10

// ---------- COMMANDS ----------
#define CMD_START        0x01
#define CMD_STATUS       0x02
#define CMD_ERROR_COUNT  0x03
#define CMD_ERROR_VALUE  0x04

// ---------- STATUS ----------
#define ST_IDLE          0x00
#define ST_COLLECTING    0x01
#define ST_DONE          0x02

// ---------- SENSOR ----------
TwoWire I2CBUS2=TwoWire(1);
Adafruit_TCS34725 tcs(
  TCS34725_INTEGRATIONTIME_154MS,
  TCS34725_GAIN_4X
);

// ---------- WS2812 ----------
#define WS2812_PIN    23
#define WS2812_COUNT  1
Adafruit_NeoPixel strip(WS2812_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ---------- GLOBAL STATE ----------
volatile uint8_t lastCmd = 0;
volatile uint8_t status = ST_IDLE;
volatile bool startRequested = false;


// store detected color as ID (0=unknown,1=R,2=G,3=B,4=Y,5=M,6=C,7=W,8=K)
uint8_t detected[SAMPLES];
uint8_t errorCount = 0;

// ---------- COLOR TABLE ----------
uint8_t colors[SAMPLES][3] = {
  {255,0,0},{0,255,0},{0,0,255},{255,0,0},{0,255,0},
  {0,0,255},{255,0,0},{0,255,0},{0,0,255},{0,255,0}
};

uint8_t expectedColorID[10] = {
  1,2,3,1,2,3,1,2,3,2
};

// ---------- COLOR DETECTION ----------
uint8_t detectColorID(float r, float g, float b) {
  if (r > 0.4 && g < 0.3 && b < 0.3) return 1; // Red
  if (r < 0.3 && g > 0.4 && b < 0.3) return 2; // Green
  if (r < 0.3 && g < 0.3 && b > 0.4) return 3; // Blue
  if (r > 0.4 && g > 0.4 && b < 0.3) return 4; // Yellow
  if (r > 0.4 && g < 0.3 && b > 0.4) return 5; // Magenta
  if (r < 0.3 && g > 0.4 && b > 0.4) return 6; // Cyan
  if (r > 0.35 && g > 0.35 && b > 0.35) return 7; // White
  if (r < 0.2 && g < 0.2 && b < 0.2) return 8; // Black
  return 0; // Unknown
}

void onReceive(int len) {
  if (!I2CBUS2.available()) return;

  lastCmd = I2CBUS2.read();

  if (lastCmd == CMD_START && status != ST_COLLECTING) {
    startRequested = true;
    errorCount = 0;
  }
}

void onRequest() {
  if (lastCmd == CMD_STATUS) {
    I2CBUS2.write(status);
  }
  else if (lastCmd == CMD_ERROR_COUNT) {
    I2CBUS2.write(errorCount);
  }
  lastCmd=0;
}

void Monitoring() {
   uint16_t r,g,b,c;
  for (int i=0;i<SAMPLES;i++) {
    strip.setPixelColor(0, strip.Color(
      colors[i][0], colors[i][1], colors[i][2]
    ));
    strip.show();

    vTaskDelay(250 / portTICK_PERIOD_MS);

    tcs.getRawData(&r,&g,&b,&c);
    if (c == 0) c = 1;

    float rn = (float)r / c;
    float gn = (float)g / c;
    float bn = (float)b / c;

    detected[i] = detectColorID(rn,gn,bn);
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
    // Turn LED OFF
strip.clear();
strip.show();
}

void Diagnostics() {
  errorCount = 0;

  for (int i=0;i<SAMPLES;i++) {
    if (detected[i] != expectedColorID[i]) {
      errorCount++;
      }
    }
}

void setup() {
      Serial.begin(115200);
// MAIN I2C BUS → Sensors
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  if (!tcs.begin(0x29)) {
    Serial.println(" No TCS34725 found!");
  }
  else
  {
    Serial.println("TCS34725 found");
  }
  
  I2CBUS2.begin(SLAVE_ADDR,I2CBUS2_SDA,I2CBUS2_SCL,100000);
  I2CBUS2.onReceive(onReceive);
  I2CBUS2.onRequest(onRequest);
  strip.begin();
  strip.show();
  uint16_t r,g,b,c;
  tcs.getRawData(&r,&g,&b,&c);
}

void loop() {
if (startRequested) {
    startRequested = false;


    status = ST_COLLECTING;

    Monitoring();
    Diagnostics();
    status = ST_DONE;
  }
}





















