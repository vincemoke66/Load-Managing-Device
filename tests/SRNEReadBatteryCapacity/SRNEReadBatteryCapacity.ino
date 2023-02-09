#include <SoftwareSerial.h>

#define RS485_RX_PIN 7
#define RS485_TX_PIN 6

#define BAUD_RATE 9600

// Define the request to send to the SRNE controller
const byte request[] = {0x01, 0x03, 0x0100, 0x0001, 0x85, 0xF6};

const int requestLength = 6;

const int maxResponseLength = 20;

SoftwareSerial RS485Serial(RS485_RX_PIN, RS485_TX_PIN);

void setup() {
  Serial.begin(BAUD_RATE);
  RS485Serial.begin(BAUD_RATE);
}

void loop() {
  // Send the request to the SRNE controller
  for (int i = 0; i < requestLength; i++) {
    RS485Serial.write(request[i]);
  }

  delay(100);

  // Read the response from the SRNE controller
  int responseLength = 0;
  byte response[maxResponseLength];
  while (RS485Serial.available() > 0 && responseLength < maxResponseLength) {
    response[responseLength++] = RS485Serial.read();
  }

  // Parse the response from the SRNE controller
  int batteryCapacity = 0;
  if (responseLength >= 6) {
    if (response[0] == 0x01 && response[1] == 0x03 && response[2] == 0x02) {
      batteryCapacity = response[3] << 8 | response[4];
    }
  }

  Serial.println(batteryCapacity);

  delay(1000);
}
