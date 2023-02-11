#include <LiquidCrystal_I2C.h>
#include <AltSoftSerial.h>
#include <ModbusMaster.h>
#include "EmonLib.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); 

// CURRENT SENSOR PINS
const int LOW_LOAD_CS = 0;
const int MID_LOAD_CS = 1;
const int HIGH_LOAD_CS = 2;

// LOAD RELAY PINS
const int LOW_LOAD_RELAY = 4;
const int MID_LOAD_RELAY = 3;
const int HIGH_LOAD_RELAY = 2;

// RS485 PINS
const int RS485_DE = 6;
const int RS485_RE_NEG = 7;

// VOLTAGE SENSOR PIN
#define VOLTAGE_SENSOR A3

// RESET BUTTON
const int RESET_BTN = 1;

// LOAD POWER LIMITS
const int LOW_LIMIT = 10;
const int MID_LIMIT = 600;
const int HIGH_LIMIT = 1000;

// LOAD CURRENT READINGS
double low_load_current = 0;
double mid_load_current = 0;
double high_load_current = 0;

// CURRENT CALIBRATION
double current_calibration = 0.00714;

// VOLTAGE VARIABLES
int raw_system_voltage = 0;        
double calculated_system_voltage = 0.0;      
double system_voltage_calibration = 0.0;

// LOAD POWER 
double low_load_power = 0;
double mid_load_power = 0;
double high_load_power = 0;

const int BAUD_RATE = 9600;

// TIME VARIABLES
const int DATA_INTERVAL = 1000;
const int INIT_TIME = 10000;
unsigned long start_time = 0;
unsigned long current_time = 0;

// CONTROLLER VARS
int battery_capacity = 0;
double charging_current = 0;
double panel_voltage = 0;
double panel_current = 0;
double charging_power = 0;

const int BATTERY_CAPACITY_LIMIT = 50;

// CURRENT SENSOR OBJECTS
EnergyMonitor emon1; 
EnergyMonitor emon2; 
EnergyMonitor emon3; 

// SRNE CONTROLLER OBJECTS
AltSoftSerial swSerial;
ModbusMaster node;

void setup() {
    Serial.begin(BAUD_RATE);

    pinMode(RS485_DE, OUTPUT);
    pinMode(RS485_RE_NEG, OUTPUT);
    digitalWrite(RS485_RE_NEG, 0);
    digitalWrite(RS485_DE, 0);

    // Modbus comms
    swSerial.begin(BAUD_RATE);
    node.begin(1, swSerial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // assigns current sensors to emon objects
    emon1.current(LOW_LOAD_CS, 111.1);
    emon2.current(MID_LOAD_CS, 111.1);
    emon3.current(HIGH_LOAD_CS, 111.1);

    // sets load relay as output
    pinMode(LOW_LOAD_RELAY, OUTPUT);
    pinMode(MID_LOAD_RELAY, OUTPUT);
    pinMode(HIGH_LOAD_RELAY, OUTPUT);

    // sets reset button as input
    pinMode(RESET_BTN, INPUT);

    // lcd initialization
    lcd.init();
    lcd.backlight();

    showInitScreen();
    delay(2000);

    allowLoadPower();
}

void loop() {
    start_time = millis(); 

    readSystemVoltage();
    readAllLoadCurrent();
    readSRNERegisters();
    calculateAllLoadPower(); 

    if (digitalRead(RESET_BTN) == 1 && battery_capacity > BATTERY_CAPACITY_LIMIT)
        allowLoadPower();

    if (start_time > INIT_TIME) {
        tripLoads();
    }

    showAllReadings();
    // TODO : add proper display 
    // TODO : add changing display of loads base on its load relay
        // display the values if relay is 1
        // display "Battery and panels can't power" if relay is 0 and source can't power
        // display "Battery and panels can't power" if relay is 0 and source can't power

}

void showInitScreen() {
    lcd.clear();
    lcd.setCursor(3, 1);
    lcd.print("LOAD MANAGING"); 
    lcd.setCursor(6, 1);
    lcd.print("DEVICE");
}

void readAllLoadCurrent() {
    low_load_current = emon1.calcIrms(1480) * current_calibration; 
    mid_load_current = emon2.calcIrms(1480) * current_calibration; 
    high_load_current = emon3.calcIrms(1480) * current_calibration; 
}

void readSystemVoltage() {
    raw_system_voltage = analogRead(VOLTAGE_SENSOR);
    system_voltage_calibration = 220 / raw_system_voltage;

    calculated_system_voltage = raw_system_voltage * system_voltage_calibration;
}

void calculateAllLoadPower() {
    low_load_power = low_load_current * calculated_system_voltage;
    mid_load_power = mid_load_current * calculated_system_voltage;
    high_load_power = high_load_current * calculated_system_voltage;
}

// trips a relay if the maximum load has reached
void tripLoads() {
    // load conditions
    if (low_load_power > LOW_LIMIT) digitalWrite(LOW_LOAD_RELAY, 0);
    if (mid_load_power > MID_LIMIT) digitalWrite(MID_LOAD_RELAY, 0);
    if (high_load_power > HIGH_LIMIT) digitalWrite(HIGH_LOAD_RELAY, 0);

    // controller conditions
    if (battery_capacity < 50) turnOffAllLoadPower();
}

void showAllReadings() {
    clrLcdValuePlaceholders();

    // voltage 
    lcd.setCursor(0, 0);
    lcd.print("VOLTAGE INPUT:");
    lcd.setCursor(14, 0);
    lcd.print(calculated_system_voltage);
    lcd.setCursor(19, 0);
    lcd.print("V");

    // loads
    showLoadReading("LOW ", 1, low_load_current, low_load_power);
    showLoadReading("MID ", 2, mid_load_current, mid_load_power);
    showLoadReading("HIGH", 3, high_load_current, high_load_power);
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

void readSRNERegisters() {
    uint8_t result;

    // PARSED DATA
    result = node.readHoldingRegisters(0x0100, 9);
    if (result == node.ku8MBSuccess)
    {
        // reading of desired registers 
        battery_capacity = node.getResponseBuffer(0x00);
        charging_current = node.getResponseBuffer(0x02) * 0.01f;

        panel_voltage = node.getResponseBuffer(0x07) * 0.1f;
        panel_current = node.getResponseBuffer(0x08) * 0.01f;
        charging_power = node.getResponseBuffer(0x09);

        // serial output of the register values
        Serial.print("Battery Capacity: ");
        Serial.print(battery_capacity);
        Serial.println(" %");

        Serial.print("Charging Current: ");
        Serial.print(charging_current);
        Serial.println(" A");

        Serial.print("Panel Volts: ");
        Serial.print(panel_voltage);
        Serial.println(" V");

        Serial.print("Panel Current: ");
        Serial.print(panel_current);
        Serial.println(" A");

        Serial.print("Charging Power: ");
        Serial.print(charging_power);
        Serial.println(" W");
    }

    Serial.println();
}

// SRNE Controller function
void preTransmission()
{
    digitalWrite(MAX485_RE_NEG, 1);
    digitalWrite(MAX485_DE, 1);
}

// SRNE Controller function
void postTransmission()
{
    digitalWrite(MAX485_RE_NEG, 0);
    digitalWrite(MAX485_DE, 0);
}

void allowLoadPower() 
{
    digitalWrite(LOW_LOAD_RELAY, 1);
    digitalWrite(MID_LOAD_RELAY, 1);
    digitalWrite(HIGH_LOAD_RELAY, 1);
}

void turnOffAllLoadPower() 
{
    digitalWrite(LOW_LOAD_RELAY, 0);
    digitalWrite(MID_LOAD_RELAY, 0);
    digitalWrite(HIGH_LOAD_RELAY, 0);
}