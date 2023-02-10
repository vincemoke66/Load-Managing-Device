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
double lLoadCurr = 0;
double mLoadCurr = 0;
double hLoadCurr = 0;

// CURRENT CALIBRATION
double calibCurrent = 0.00714;

// VOLTAGE VARIABLES
float rawVolts = 0;        // Analog Input
double calcVolts = 0.0;      // Actual voltage after calculation
double calibVolts = 0.0;


// LOAD POWER 
double lLoadPower = 0;
double mLoadPower = 0;
double hLoadPower = 0;

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

    lLoadPower = 0;
    mLoadPower = 0;
    hLoadPower = 0;

    // assigns current sensors to emon objects
    emon1.current(LOW_LOAD_CS, 111.1);
    emon2.current(MID_LOAD_CS, 111.1);
    emon3.current(HIGH_LOAD_CS, 111.1);

    // sets load relay as output
    pinMode(LOW_LOAD_RELAY, OUTPUT);
    pinMode(MID_LOAD_RELAY, OUTPUT);
    pinMode(HIGH_LOAD_RELAY, OUTPUT);

    // lcd initialization
    lcd.init();
    lcd.backlight();

    showInitScreen();
    delay(2000);

    start_time = millis(); 

    // opens the connection from source to load
    digitalWrite(LOW_LOAD_RELAY, 1);
    digitalWrite(MID_LOAD_RELAY, 1);
    digitalWrite(HIGH_LOAD_RELAY, 1);
}

void loop() {
    start_time = millis(); 

    Serial.print("Start time: ");
    Serial.println(start_time);
    // if (start_time - current_time > monitor_interval) {
    if (start_time > init_time) {
        Serial.println("perform after init time");
        tripLoads();
    }

    readVoltage();
    readAllLoadCurrent();
    calculateAllLoadPower(); 

    showAllReadings();


    //     current_time = start_time;
    // }
}

void showInitScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TESTING FEATURE:");
    lcd.setCursor(0, 1);
    lcd.print("Power Tripper");
}

void readAllLoadCurrent() {
    double currentNegligence = 0.009;

    lLoadCurr = emon1.calcIrms(1480) * calibCurrent; 
    mLoadCurr = emon2.calcIrms(1480) * calibCurrent; 
    hLoadCurr = emon3.calcIrms(1480) * calibCurrent; 

    if (lLoadCurr <= currentNegligence) lLoadCurr = 0;
    if (mLoadCurr <= currentNegligence) mLoadCurr = 0;
    if (hLoadCurr <= currentNegligence) hLoadCurr = 0;
}

void readVoltage() {
    rawVolts = analogRead(VOLTAGE_SENSOR);
    calibVolts = 220 / rawVolts;

    calcVolts = rawVolts * calibVolts;
}

void calculateAllLoadPower() {
    lLoadPower = lLoadCurr * calcVolts;
    mLoadPower = mLoadCurr * calcVolts;
    hLoadPower = hLoadCurr * calcVolts;
}

// trips a relay if the maximum load has reached
void tripLoads() {
    if (lLoadPower > LOW_LIMIT) digitalWrite(LOW_LOAD_RELAY, 0);
    if (mLoadPower > MID_LIMIT) digitalWrite(MID_LOAD_RELAY, 0);
    if (hLoadPower > HIGH_LIMIT) digitalWrite(HIGH_LOAD_RELAY, 0);
}

void showAllReadings() {
    clrLcdValuePlaceholders();

    // voltage 
    lcd.setCursor(0, 0);
    lcd.print("VOLTAGE INPUT:");
    lcd.setCursor(14, 0);
    lcd.print(calcVolts);
    lcd.setCursor(19, 0);
    lcd.print("V");

    // loads
    showLoadReading("LOW ", 1, lLoadCurr, lLoadPower);
    showLoadReading("MID ", 2, mLoadCurr, mLoadPower);
    showLoadReading("HIGH", 3, hLoadCurr, hLoadPower);
}

void showLoadReading(char *loadLabel, int row, double loadCurr, double loadWatts) {
    lcd.setCursor(0, row);
    lcd.print(loadLabel);
    lcd.print(":");
    lcd.setCursor(5, row);
    lcd.print(loadCurr);
    lcd.setCursor(9, row);
    lcd.print("A"); 

    lcd.setCursor(10, row);
    lcd.print("||");

    lcd.setCursor(12, row);
    lcd.print(loadWatts);
    lcd.setCursor(19, row);
    lcd.print("W");
}

void clrLcdValuePlaceholders() {
    // voltage placeholder
    lcd.setCursor(14, 0);
    lcd.print("     ");

    // fill in blank text on load placeholders
    for (int i = 1; i < 4; i++) {
        // current placeholder
        lcd.setCursor(5, i);
        lcd.print("    ");
        // power placeholder
        lcd.setCursor(12, i);
        lcd.print("       ");
    }
}