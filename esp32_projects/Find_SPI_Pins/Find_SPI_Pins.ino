/*
  ESP32 SPI Pin Finder
  --------------------
  Prints the default SPI pin mapping for your selected ESP32 board.

  Make sure to select the correct board under:
  Tools → Board → (Your ESP32 model)

  Then open the Serial Monitor (115200 baud) and reset the board.

  Author: Rui Santos (RandomNerdTutorials.com)
  Modified by ChatGPT (with comments and formatting)
*/

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for Serial Monitor to open

  Serial.println();
  Serial.println("====================================");
  Serial.println("  ESP32 Default SPI Pin Information ");
  Serial.println("====================================");

  Serial.print("MOSI (Master Out Slave In): ");
  Serial.println(MOSI);
  Serial.print("MISO (Master In Slave Out): ");
  Serial.println(MISO);
  Serial.print("SCK  (Serial Clock): ");
  Serial.println(SCK);
  Serial.print("SS   (Slave Select / CS): ");
  Serial.println(SS);

  Serial.println("====================================");
  Serial.println("Make sure you selected the right board in Tools > Board.");
  Serial.println("If pins show -1, your board definition may not specify them.");
  Serial.println("====================================");
}

void loop() {
  // Nothing to do repeatedly
  Serial.println("fdfsf");
}
