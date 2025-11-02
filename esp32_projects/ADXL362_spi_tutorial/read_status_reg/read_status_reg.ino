#include <SPI.h>

#define PIN_CS   5
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n===============================");
  Serial.println("  ADXL362 STATUS Register Read ");
  Serial.println("       (Continuous Mode)        ");
  Serial.println("===============================\n");

  // Initialize SPI and CS pin
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
}

uint8_t readStatusRegister() {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x0B);          // Command: READ_REGISTER
  SPI.transfer(0x0B);          // Address: STATUS register (0x0B)
  uint8_t status = SPI.transfer(0x00);  // Read value
  digitalWrite(PIN_CS, HIGH);

  SPI.endTransaction();

  return status;
}

void printStatusBits(uint8_t status) {
  Serial.print("STATUS (0x0B) = 0x");
  Serial.print(status, HEX);
  Serial.print("  ->  ");

  Serial.print((status & 0x01) ? "DATA_READY " : "");
  Serial.print((status & 0x02) ? "FIFO_READY " : "");
  Serial.print((status & 0x04) ? "FIFO_OVERRUN " : "");
  Serial.print((status & 0x08) ? "FIFO_WATERMARK " : "");
  Serial.print((status & 0x10) ? "ACT " : "");
  Serial.print((status & 0x20) ? "INACT " : "");
  Serial.print((status & 0x40) ? "AWAKE " : "");
  Serial.print((status & 0x80) ? "ERR_USER_REGS " : "");

  Serial.println();
}

void loop() {
  uint8_t status = readStatusRegister();
  printStatusBits(status);
  delay(1000);
}
