#include <SoftwareSerial.h>

SoftwareSerial RS485Serial(8, 9); // RX, TX

const int baudRate = 9600;
const byte request[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x01, 0x85, 0xF6};
const int requestLength = 8;
byte response[10];

void setup() {
  RS485Serial.begin(baudRate);
  Serial.begin(baudRate);
}

void loop() {
  // Send the request to the SRNE controller
  RS485Serial.write(request, requestLength);
  RS485Serial.flush();

  // Wait for the response from the SRNE controller
  int responseLength = RS485Serial.readBytes(response, 10);

  // Print the raw data received from the SRNE controller
  for (int i = 0; i < responseLength; i++) {
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  delay(1000);
}
