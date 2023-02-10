// This attempt uses the AltSoftSerial & ModbusMaster libraries.
// Get AltSoftSerial at https://www.pjrc.com/teensy/td_libs_AltSoftSerial.html
//
// RS485 module wired up as:
// RS485 DI signal to pin 9
// RS485 RO signal to pin 8
// RS485 RE signal to pin 7
// RS485 DE signal to pin 6
// RS485 VCC to 5V
// RS485 GND to GND
//
// NOTE: I do not have this sensor, so I simulated it using the evaluation version of WinModbus.

#include <ModbusMaster.h>
#include <AltSoftSerial.h>

#define MAX485_DE      6
#define MAX485_RE_NEG  7

AltSoftSerial swSerial;
ModbusMaster node;

void preTransmission()
{
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void setup() {
  Serial.begin( 9600 );

  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);

  // Modbus communication runs at 9600 baud
  swSerial.begin(9600);

  // Modbus slave ID of NPK sensor is 1
  node.begin(1, swSerial);

  // Callbacks allow us to configure the RS485 transceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

void loop() {
  uint8_t result;

  // PARSED DATA
  result = node.readHoldingRegisters(0x0100, 9);
  if (result == node.ku8MBSuccess)
  {
    Serial.print("   Panel Voltage: ");
    Serial.print(node.getResponseBuffer(0x07) * 0.1f);
    Serial.println(" V");

    Serial.print("   Charging Current: ");
    Serial.print(node.getResponseBuffer(0x02) * 0.01f);
    Serial.println(" A");

    Serial.print("   Battery Percentage: ");
    Serial.print(node.getResponseBuffer(0x00));
    Serial.println(" %");

    Serial.print("   Battery Voltage: ");
    Serial.print(node.getResponseBuffer(0x01) * 0.1f);
    Serial.println(" V");
  }


  Serial.println();
  delay(1000);
}