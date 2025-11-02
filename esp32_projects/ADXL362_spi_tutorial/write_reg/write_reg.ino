#include <SPI.h>

// SPI pin mapping for ESP32
#define PIN_CS   5
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

// SPI command codes
#define CMD_READ_REGISTER  0x0B
#define CMD_WRITE_REGISTER 0x0A

// ADXL362 register addresses
#define REG_DEVID_AD       0x00
#define REG_DEVID_MST      0x01
#define REG_PARTID         0x02
#define REG_REVID          0x03
#define REG_STATUS         0x0B
#define REG_POWER_CTL      0x2D

uint8_t readRegister(uint8_t addr) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(CMD_READ_REGISTER);
  SPI.transfer(addr);
  uint8_t val = SPI.transfer(0x00);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
  return val;
}

void writeRegister(uint8_t addr, uint8_t value) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(CMD_WRITE_REGISTER);
  SPI.transfer(addr);
  SPI.transfer(value);
  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();
}

void printStatus(uint8_t status) {
  Serial.print("STATUS (0x0B) = 0x");
  Serial.print(status, HEX);
  Serial.print("  ->  ");
  if (status & 0x01) Serial.print("DATA_READY ");
  if (status & 0x02) Serial.print("FIFO_READY ");
  if (status & 0x04) Serial.print("FIFO_OVERRUN ");
  if (status & 0x08) Serial.print("FIFO_WATERMARK ");
  if (status & 0x10) Serial.print("ACT ");
  if (status & 0x20) Serial.print("INACT ");
  if (status & 0x40) Serial.print("AWAKE ");
  if (status & 0x80) Serial.print("ERR_USER_REGS ");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=====================================");
  Serial.println(" ADXL362 ID + STATUS + Measure Mode ");
  Serial.println("=====================================\n");

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);

  // --- Step 0: Read identification registers once ---
  Serial.println("[Device Identification]");
  uint8_t devid_ad  = readRegister(REG_DEVID_AD);
  uint8_t devid_mst = readRegister(REG_DEVID_MST);
  uint8_t partid    = readRegister(REG_PARTID);
  uint8_t revid     = readRegister(REG_REVID);

  Serial.print("DEVID_AD (0x00): 0x");  Serial.println(devid_ad, HEX);
  Serial.print("DEVID_MST(0x01): 0x");  Serial.println(devid_mst, HEX);
  Serial.print("PARTID   (0x02): 0x");  Serial.println(partid, HEX);
  Serial.print("REVID    (0x03): 0x");  Serial.println(revid, HEX);
  Serial.println();

  // --- Step 1: Read STATUS a few times before measure mode ---
  Serial.println("[Before Measure Mode]");
  for (int i = 0; i < 5; i++) {
    uint8_t s = readRegister(REG_STATUS);
    printStatus(s);
    delay(500);
  }

  // --- Step 2: Enable Measure Mode ---
  Serial.println("\nEnabling Measure Mode...");
  writeRegister(REG_POWER_CTL, 0x02);
  delay(100);

  // --- Step 3: Read STATUS continuously ---
  Serial.println("\n[After Measure Mode Enabled]");
}

void loop() {
  uint8_t s = readRegister(REG_STATUS);
  printStatus(s);
  delay(1000);
}
