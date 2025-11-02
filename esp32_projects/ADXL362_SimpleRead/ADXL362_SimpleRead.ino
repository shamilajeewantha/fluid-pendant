#include <SPI.h>
#include <ADXL362.h>


// | ESP32 Pin   | ADXL362 Pin    | Description              |
// | ----------- | -------------- | ------------------------ |
// | **3.3V**    | **VCC**        | Power                    |
// | **GND**     | **GND**        | Ground                   |
// | **GPIO 18** | **SCLK / SCL** | SPI Clock                |
// | **GPIO 19** | **MISO / SDO** | Master-In-Slave-Out      |
// | **GPIO 23** | **MOSI / SDA** | Master-Out-Slave-In      |
// | **GPIO 5**  | **CS**         | Chip Select (active LOW) |





#define PIN_MOSI 23
#define PIN_MISO 19
#define PIN_SCK  18
#define PIN_CS   5   // change to 15 if 5 doesn’t work

ADXL362 xl;

int16_t XValue, YValue, ZValue, Temperature;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("====================================");
  Serial.println("   ADXL362 Accelerometer Reader     ");
  Serial.println("====================================");

  // Initialize SPI manually for ESP32
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(100);

  Serial.println("SPI started...");
  Serial.print("MOSI="); Serial.println(PIN_MOSI);
  Serial.print("MISO="); Serial.println(PIN_MISO);
  Serial.print("SCK ="); Serial.println(PIN_SCK);
  Serial.print("CS  ="); Serial.println(PIN_CS);

  // Start ADXL362
  Serial.println("Initializing ADXL362...");
  xl.begin(PIN_CS);    // setup SPI protocol and reset
  delay(100);

  xl.beginMeasure();   // start measurement
  delay(100);

  Serial.println("ADXL362 now measuring!");
  Serial.println("====================================");
}

void loop() {
  // Read all 3 axes and temperature
  xl.readXYZTData(XValue, YValue, ZValue, Temperature);

  // Debug log of SPI raw values
  Serial.print("X="); Serial.print(XValue);
  Serial.print("\tY="); Serial.print(YValue);
  Serial.print("\tZ="); Serial.print(ZValue);
  Serial.print("\tTemp="); Serial.print(Temperature);
  Serial.println();

  delay(500);
}
