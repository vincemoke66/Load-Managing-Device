#include <LiquidCrystal_I2C.h>
#include "EmonLib.h"
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); 

// CURRENT SENSOR OBJECTS
EnergyMonitor eMon1; // Create an instance</p>
EnergyMonitor eMon2; // Create an instance</p>
EnergyMonitor eMon3; // Create an instance</p>

// bool surge = false;

// LOAD RELAYS
int redline = 4;
int blueline = 3;
int yellowline = 2;

// LOAD POWER VARIABLES
int redLinePower = 0;
int blueLinePower = 0;
int yellowLinePower = 0;

double redLineCurrent, yellowLineCurrent, blueLineCurrent;

// AC VOLTAGE READINGS
int acvoltinput = 0;              //Analog Input
float VIn_a = 0.0;               //Voltage In after voltage divider
float Voltage_a = 0.0;           //Actual voltage after calculation
float CalVal = 11.08433;         //Voltage divider calibration value
float AC_LOW_VOLT = 0.0;        //Voltage at the secondary output of the transformer
float AC_HIGH_VOLT = 0.0;       //Voltage at the primary input of the transformer(this is the voltage reading from the lcd)

// LOAD POWER LIMITS
double LOAD1_LIMIT = 300;
double LOAD2_LIMIT = 600;
double LOAD3_LIMIT = 1000;

unsigned long delay_MS(unsigned long DELAY) // declaring and using a different delay for the code,which will allow background running of other functions
{
  unsigned long t = millis();
  while (millis() - t < DELAY) {
    controller();
  }
}

void controller() //the function to compare the preset power to the in used power for each lines
{
  if(redLinePower > LOAD1_LIMIT){
    digitalWrite(redline, 1);
  }
  
  if(blueLinePower > LOAD2_LIMIT){
    digitalWrite(blueline, 1);
  }

  if(yellowLinePower > LOAD3_LIMIT)
  {
    digitalWrite(yellowline, 1);
  }
}

void setup() 
{
  eMon1.current(1, 111.1); // assigns current sensor 1 to eMon1 
  eMon2.current(2, 111.1); // assigns current sensor 2 to eMon2 
  eMon3.current(3, 111.1); // assigns current sensor 3 to eMon3 

  redLineCurrent = eMon1.calcIrms(1480) - 0.3; // reads current on red line
  blueLineCurrent = eMon2.calcIrms(1480) - 0.3; // reads current on blue line
  yellowLineCurrent = eMon3.calcIrms(1480) - 0.3; // reads current on yellow line

  for (int x = 2; x < 5; x++) pinMode(x, OUTPUT); // sets red, yellow, and blue as output 
  for (int x = 2; x < 5; x++) digitalWrite(x, 0); // writes 0 on red, yellow, and blue 

  Serial.begin(9600);
  lcd.init();                
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("LOAD CONTROL SYSTEM!");
  delay(2000);

  lcd.setCursor(2,1);
  lcd.print("Partial/Test Setup");
  delay(2000);

  lcd.setCursor(0,3);
  lcd.print("Initialising..!");
  lcd.blink();
  delay(5000);

  redLineCurrent = eMon1.calcIrms(1480) - 0.3; // reads current on red line
  blueLineCurrent = eMon2.calcIrms(1480) - 0.3; // reads current on blue line
  yellowLineCurrent = eMon3.calcIrms(1480) - 0.3; // reads current on yellow line

  digitalWrite(redline, 0);
  digitalWrite(blueline, 0);
  digitalWrite(yellowline, 0);
}


void loop() 
{
  redLineCurrent = eMon1.calcIrms(1480) - 0.3; // reads current on red line
  blueLineCurrent = eMon2.calcIrms(1480) - 0.3; // reads current on blue line
  yellowLineCurrent = eMon3.calcIrms(1480) - 0.3; // reads current on yellow line

  if (redLineCurrent < 0.10) redLineCurrent = 0.00;
  if (blueLineCurrent < 0.10) blueLineCurrent = 0.00;
  if (yellowLineCurrent < 0.10) yellowLineCurrent = 0.00;
  
  int volt = 0;
  volt = random(215,225);

  acvoltinput = analogRead(A3);              //Read analog values
  VIn_a = (acvoltinput * 5.00) / 1024.00;     //Convert 10bit input to an actual voltage
  Voltage_a = (VIn_a * CalVal);
  AC_LOW_VOLT = (Voltage_a / 1.414 );
  AC_HIGH_VOLT = ( AC_LOW_VOLT * 18.333) + 40;

  // calculatung power of each lines
  redLinePower = ( volt * redLineCurrent);
  blueLinePower = ( volt * blueLineCurrent);
  yellowLinePower = ( volt * yellowLineCurrent);

  // if (surge == false)
  // {
  //   redLineCurrent = 0.00;
  //   blueLineCurrent = 0.00;
  //   yellowLineCurrent = 0.00; 
  //   delay(5000);
  //   surge = true;
  // }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("VOLTAGE INPUT:");
  lcd.setCursor(14, 0);
  lcd.print(volt);
  lcd.setCursor(18, 0);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("Rlc:");
  lcd.setCursor(4, 1);
  lcd.print(redLineCurrent);
  lcd.setCursor(8, 1);
  lcd.print("A");
  lcd.setCursor(10, 1);
  lcd.print("Rlp:");
  lcd.setCursor(14, 1);
  lcd.print(redLinePower);
  lcd.setCursor(18, 1);
  lcd.print("W");

  lcd.setCursor(0, 2);
  lcd.print("Blc:");
  lcd.setCursor(4, 2);
  lcd.print(blueLineCurrent);
  lcd.setCursor(8, 2);
  lcd.print("A");
  lcd.setCursor(10, 2);
  lcd.print("Blp:");
  lcd.setCursor(14, 2);
  lcd.print(blueLinePower);
  lcd.setCursor(18, 2);
  lcd.print("W");
  lcd.setCursor(0, 3);
  lcd.print("Ylc:");
  lcd.setCursor(4, 3);
  lcd.print(yellowLineCurrent);
  lcd.setCursor(8, 3);
  lcd.print("A");
  lcd.setCursor(10, 3);
  lcd.print("Ylp:");
  lcd.setCursor(14, 3);
  lcd.print(yellowLinePower);
  lcd.setCursor(18, 3);
  lcd.print("W");

  redLineCurrent = eMon1.calcIrms(1480) - 0.1; // Calculate RED LINE only
  blueLineCurrent = eMon2.calcIrms(1480) - 0.1; // Calculate BLUE LINE only
  yellowLineCurrent = eMon3.calcIrms(1480) - 0.1; // Calculate YELLOW LINE only

  delay_MS(1500);
}