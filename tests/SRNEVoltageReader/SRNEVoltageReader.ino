#include <SoftwareSerial.h> 
#define RS485_RX 7
#define RS485_TX 6
SoftwareSerial RS485Serial(RS485_RX, RS485_TX); 

void setup() {
  Serial.begin(9600); 
  RS485Serial.begin(9600); 
}

void loop() {
  float solarVoltage; 
  char buffer[100]; 
  int bytesRead = 0; 
  
  // Send the request to the SRNE controller to read the solar panel voltage
  RS485Serial.write("#0105\r"); 
  
  // Read the response from the SRNE controller
  bytesRead = RS485Serial.readBytesUntil('\r', buffer, 100); 
  buffer[bytesRead] = '\0'; 
  
  // Extract the solar voltage from the response
  sscanf(buffer, "#%*d05%f", &solarVoltage); 
  
  // Print the solar voltage
  Serial.println("Solar panel voltage: " + String(solarVoltage) + " V"); 
  
  delay(1000); 
}
