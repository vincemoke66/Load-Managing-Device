#include <LiquidCrystal_I2C.h>
#inlcude "EmonLib.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); 

// CURRENT SENSOR PINS
#define LOW_LOAD_CS 0
#define MID_LOAD_CS 1
#define HIGH_LOAD_CS 2

// CURRENT SENSOR OBJECTS
EnergyMonitor eMon1; 
EnergyMonitor eMon2; 
EnergyMonitor eMon3; 

void setup() {
    lcd.init();                
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("LC: ");

    lcd.setCursor(0, 0);
    lcd.print("LC: ");

    lcd.setCursor(0, 0);
    lcd.print("LC: ");

}

void loop() {

}