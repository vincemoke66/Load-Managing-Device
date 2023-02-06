#include <LiquidCrystal_I2C.h>
#include "EmonLib.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); 

// CURRENT SENSOR PINS
#define LOW_LOAD_CS     0
#define MID_LOAD_CS     1
#define HIGH_LOAD_CS    2

// LOAD RELAY PINS
#define LOW_LOAD_RELAY  4
#define MID_LOAD_RELAY  3
#define HIGH_LOAD_RELAY 2

// VOLTAGE SENSOR PIN
#define VOLTAGE_SENSOR A3

// LOAD POWER LIMITS
const int LOW_LIMIT = 10;
const int MID_LIMIT = 600;
const int HIGH_LIMIT = 1000;

// LOAD CURRENT READINGS
double lowLoadCurrent = 0;
double midLoadCurrent = 0;
double highLoadCurrent = 0;

// CURRENT CALIBRATION
double currentCalibration = 0.00714;

// VOLTAGE VARIABLES
int rawVoltage = 0;        // Analog Input
double calculatedVoltage = 0.0;      // Actual voltage after calculation
// VOLTAGE CALIBRATION CASES
double vc330 = 0.67;    
double vc280 = 0.785714;    


// LOAD POWER 
double lowLoadPower = 0;
double midLoadPower = 0;
double highLoadPower = 0;

// CURRENT SENSOR OBJECTS
EnergyMonitor emon1; 
EnergyMonitor emon2; 
EnergyMonitor emon3; 

// TIME VARIABLES
int monitor_interval = 2000;
int init_time = 10000;
unsigned long start_time = 0;
unsigned long current_time = 0;

void setup() {
    Serial.begin(9600);
    start_time = millis(); 

    // assigns current sensors to emon objects
    emon1.current(LOW_LOAD_CS, 111.1);
    emon2.current(MID_LOAD_CS, 111.1);
    emon3.current(HIGH_LOAD_CS, 111.1);

    readAllLoadCurrent();

    // sets load relay as output
    pinMode(LOW_LOAD_RELAY, OUTPUT);
    pinMode(MID_LOAD_RELAY, OUTPUT);
    pinMode(HIGH_LOAD_RELAY, OUTPUT);

    // opens the connection from source to load
    digitalWrite(LOW_LOAD_RELAY, 1);
    digitalWrite(MID_LOAD_RELAY, 1);
    digitalWrite(HIGH_LOAD_RELAY, 1);

    // lcd initialization
    lcd.init();
    lcd.backlight();

    showWelcomeScreen();
    delay(2000);

    // start_time = millis();

    showAllReadings();

    lowLoadPower = 0;
    midLoadPower = 0;
    highLoadPower = 0;

    start_time = millis(); 
}

void showWelcomeScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TESTING FEATURE:");
    lcd.setCursor(0, 1);
    lcd.print("Power Tripper");
}

void readAllLoadCurrent() {
    lowLoadCurrent = emon1.calcIrms(1480) * currentCalibration; 
    midLoadCurrent = emon2.calcIrms(1480) * currentCalibration; 
    highLoadCurrent = emon3.calcIrms(1480) * currentCalibration; 
}

void readVoltage() {
    rawVoltage = analogRead(VOLTAGE_SENSOR) ;   // Read analog values
    if (rawVoltage > 290) {
        calculatedVoltage = (rawVoltage * vc330);
    } else {
        calculatedVoltage = (rawVoltage * vc280);
    }
}

void calculateAllLoadPower() {
    lowLoadPower = lowLoadCurrent * calculatedVoltage;
    midLoadPower = midLoadCurrent * calculatedVoltage;
    highLoadPower = highLoadCurrent * calculatedVoltage;
}

void loop() {
    start_time = millis(); 

    Serial.print("Start time: ");
    Serial.println(start_time);
    // if (start_time - current_time > monitor_interval) {
    if (start_time > init_time) {
        Serial.println("perform after init time");
        loadTripper();
    }

    readVoltage();
    readAllLoadCurrent();
    calculateAllLoadPower(); 

    showAllReadings();


    //     current_time = start_time;
    // }
}

void loadTripper() {
    if (lowLoadPower > LOW_LIMIT) digitalWrite(LOW_LOAD_RELAY, 0);
    if (midLoadPower > MID_LIMIT) digitalWrite(MID_LOAD_RELAY, 0);
    if (highLoadPower > HIGH_LIMIT) digitalWrite(HIGH_LOAD_RELAY, 0);
}

void showAllReadings() {
    lcd.clear();

    // voltage 
    lcd.setCursor(0, 0);
    lcd.print("VOLTAGE INPUT:");
    lcd.setCursor(14, 0);
    lcd.print(calculatedVoltage);
    lcd.setCursor(19, 0);
    lcd.print("V");

    // low load
    lcd.setCursor(0, 1);
    lcd.print("LOW:");
    lcd.setCursor(5, 1);
    lcd.print(lowLoadCurrent);
    lcd.setCursor(9, 1);
    lcd.print("A");
    //
    lcd.setCursor(11, 1);
    lcd.print("||");
    //
    lcd.setCursor(14, 1);
    lcd.print(lowLoadPower);
    lcd.setCursor(19, 1);
    lcd.print("W");

    // mid load
    lcd.setCursor(0, 2);
    lcd.print("MID:");
    lcd.setCursor(5, 2);
    lcd.print(midLoadCurrent);
    lcd.setCursor(9, 2);
    lcd.print("A");
    //
    lcd.setCursor(11, 2);
    lcd.print("||");
    //
    lcd.setCursor(14, 2);
    lcd.print(midLoadPower);
    lcd.setCursor(19, 2);
    lcd.print("W");

    // high load
    lcd.setCursor(0, 3);
    lcd.print("HIGH:");
    lcd.setCursor(5, 3);
    lcd.print(highLoadCurrent);
    lcd.setCursor(9, 3);
    lcd.print("A");
    //
    lcd.setCursor(11, 3);
    lcd.print("||");
    //
    lcd.setCursor(14, 3);
    lcd.print(rawVoltage);
    lcd.setCursor(19, 3);
    lcd.print("V");
}