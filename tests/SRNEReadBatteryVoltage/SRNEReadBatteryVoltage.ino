#include <SoftwareSerial.h> 
#define RS485_RX 7
#define RS485_TX 6
SoftwareSerial RS485Serial(RS485_RX, RS485_TX); 

void setup() {
  Serial.begin(9600); 
  RS485Serial.begin(9600); 
}

void loop() {
  float batteryVoltage; 
  char buffer[100]; 
  int bytesRead = 0; 
  
  // Send the request to the SRNE controller to read the battery voltage
  RS485Serial.write("01 03 0101 0001 D436"); 
  
  // Read the response from the SRNE controller
  bytesRead = RS485Serial.readBytesUntil('\r', buffer, 100); 
  buffer[bytesRead] = '\0'; 
  
  // Extract the battery voltage from the response
  sscanf(buffer, "01 %*d 0101 %*d %f", &batteryVoltage); 
  
  // Print the battery voltage
  Serial.println("Battery voltage: " + String(batteryVoltage) + " V"); 
  
  delay(1000); 
}
