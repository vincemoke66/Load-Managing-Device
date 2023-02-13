#include <LiquidCrystal_I2C.h>
#include <AltSoftSerial.h>
#include <ModbusMaster.h>
#include "EmonLib.h"

// PINS
const int LOW_LOAD_CS = 0;
const int MID_LOAD_CS = 1;
const int HIGH_LOAD_CS = 2;

const int LOW_RELAY = 4;
const int MID_RELAY = 3;
const int HIGH_RELAY = 2;

const int RESET_BUTTON = 5;

const int RS485_DE = 6;
const int RS485_RE_NEG = 7;

#define VOLTAGE_SENSOR A3

// Current and Voltage variables
double raw_voltage = 0;
double system_voltage = 0;

double low_load_power = 0;
double mid_load_power = 0;
double high_load_power = 0;

double low_load_current = 0;
double mid_load_current = 0;
double high_load_current = 0;

double current_calibration = 0.00714;

// SRNE register variables
double battery_capacity;
double charging_current;
double panel_voltage;
double panel_current;
double charging_power;

// Objects 
LiquidCrystal_I2C lcd(0x27, 20, 4); 
EnergyMonitor emon1; 
EnergyMonitor emon2; 
EnergyMonitor emon3; 
AltSoftSerial swSerial;
ModbusMaster node;

// Limit Variables
const int SOURCE_BATTERY_LIMIT = 50;
const int LOW_POWER_LIMIT = 300;
const int MID_POWER_LIMIT = 600;
const int HIGH_POWER_LIMIT = 1000;

bool isLowLoadBlocked = false;
bool isMidLoadBlocked = false;
bool isHighLoadBlocked = false;

// Time variables
const int INITIALIZATION_TIME = 10000;
unsigned long start_time = 0;

// Baud rate
const int BAUD_RATE = 9600;

void setup() {
    Serial.begin(BAUD_RATE);

    // RS485 init
    pinMode(RS485_DE, OUTPUT);
    pinMode(RS485_RE_NEG, OUTPUT);
    postTransmission();

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
    pinMode(LOW_RELAY, OUTPUT);
    pinMode(MID_RELAY, OUTPUT);
    pinMode(HIGH_RELAY, OUTPUT);

    // sets reset button as input
    pinMode(RESET_BUTTON, INPUT);

    lcd.init();
    lcd.backlight();

    showInitScreen();

    allowLoadPower();
}

void loop() {
    // starts millis time for tracking initialization time
    start_time = millis();

    // readings of all necessary data
    readSRNERegisters();
    readSystemVoltage();
    readLoadPowerConsumption();

    // blocks other processes while initializing
    if (start_time < INITIALIZATION_TIME) return;

    // handles reset button click event
    if (digitalRead(RESET_BUTTON) == HIGH && sourceCanSupply()) {
        allowLoadPower();
    }

    // displays all readings after all readings
    // and possible load trips
    displayAllReadings();
    tripLoad();
}

void allowLoadPower() {
    digitalWrite(LOW_RELAY, 1);
    digitalWrite(MID_RELAY, 1);
    digitalWrite(HIGH_RELAY, 1);

    isLowLoadBlocked = false;
    isMidLoadBlocked = false;
    isHighLoadBlocked = false;
}

void showInitScreen() {
    lcd.clear();
    lcd.setCursor(3, 1);
    lcd.print("LOAD MANAGING"); 
    lcd.setCursor(6, 2);
    lcd.print("DEVICE"); 
    delay(2000);

    lcd.clear();
    lcd.setCursor(3, 1);
    lcd.print("INITIALIZING.."); 
    lcd.setCursor(4, 2);
    lcd.print("PLEASE WAIT"); 
}

// allows all loads to consume back power
void reset() {
    digitalWrite(LOW_RELAY, 1);
    digitalWrite(MID_RELAY, 1);
    digitalWrite(HIGH_RELAY, 1);
}

void readSRNERegisters() {
    uint8_t result;

    // PARSED DATA
    result = node.readHoldingRegisters(0x0100, 9);
    if (result == node.ku8MBSuccess) {
        // reading of desired registers 
        battery_capacity = node.getResponseBuffer(0x00);
        charging_current = node.getResponseBuffer(0x02) * 0.01f;
        panel_voltage = node.getResponseBuffer(0x07) * 0.1f;
        panel_current = node.getResponseBuffer(0x08) * 0.01f;
        charging_power = node.getResponseBuffer(0x09);
    }
}

bool sourceCanSupply() {
    return battery_capacity > SOURCE_BATTERY_LIMIT;
}

// SRNE Controller functions
void preTransmission()
{
    digitalWrite(RS485_RE_NEG, 1);
    digitalWrite(RS485_DE, 1);
}

void postTransmission()
{
    digitalWrite(RS485_RE_NEG, 0);
    digitalWrite(RS485_DE, 0);
}

void readSystemVoltage() {
    raw_voltage = analogRead(VOLTAGE_SENSOR);
    // system_voltage = ( raw_voltage / 1023.0 ) * 5.0;
    system_voltage = raw_voltage;
}

void readLoadPowerConsumption() {
    // load current
    low_load_current = emon1.calcIrms(1480) * current_calibration; 
    mid_load_current = emon2.calcIrms(1480) * current_calibration; 
    high_load_current = emon3.calcIrms(1480) * current_calibration; 

    // load power
    low_load_power = low_load_current * system_voltage;
    mid_load_power = mid_load_current * system_voltage;
    high_load_power = high_load_current * system_voltage;
}

void tripLoad() {
    if (!sourceCanSupply()) {
        digitalWrite(LOW_RELAY, 0);
        digitalWrite(MID_RELAY, 0);
        digitalWrite(HIGH_RELAY, 0);

        isLowLoadBlocked = true;
        isMidLoadBlocked = true;
        isHighLoadBlocked = true;

        // clear the loads power 
        for (int r = 1; r < 4; r++) {
            lcd.setCursor(r, 5);
            lcd.print("               ");
        }

        // display "OUT OF POWER"
        lcd.setCursor(6, 2);
        lcd.print("OUT OF POWER");
        return;
    }

    if (low_load_power > LOW_POWER_LIMIT && !isLowLoadBlocked) {
        digitalWrite(LOW_RELAY, 0);
        isLowLoadBlocked = true;
        lcd.setCursor(4, 1);
        lcd.print("               ");
        lcd.setCursor(5, 1);
        lcd.print("LIMIT EXCEEDED!");
    }
    if (mid_load_power > MID_POWER_LIMIT && !isMidLoadBlocked) {
        digitalWrite(MID_RELAY, 0);
        isMidLoadBlocked = true;
        lcd.setCursor(4, 2);
        lcd.print("               ");
        lcd.setCursor(5, 2);
        lcd.print("LIMIT EXCEEDED!");
    }
    if (high_load_power > HIGH_POWER_LIMIT && !isHighLoadBlocked) {
        digitalWrite(HIGH_RELAY, 0);
        isHighLoadBlocked = true;
        lcd.setCursor(4, 3);
        lcd.print("               ");
        lcd.setCursor(5, 3);
        lcd.print("LIMIT EXCEEDED!");
    }
}

void displayAllReadings() {
    lcd.setCursor(0, 0);
    lcd.print("VOLTAGE INPUT:");
    lcd.setCursor(14, 0);
    lcd.print(system_voltage);
    lcd.setCursor(19, 0);
    lcd.print("V");

    double loadCurrent = 0;
    double loadPower = 0;

    for (int i = 1; i < 4; i++) {
        if (i == 1 && isLowLoadBlocked) continue;
        if (i == 2 && isMidLoadBlocked) continue;
        if (i == 3 && isHighLoadBlocked) continue;

        lcd.setCursor(0, i);
        lcd.print("                    ");

        lcd.setCursor(0, i);

        if (i == 1) {
            lcd.print("LOW :");
            loadCurrent = low_load_current;
            loadPower  = low_load_power;
        } 
        if (i == 2) {
            lcd.print("MID :");
            loadCurrent = mid_load_current;
            loadPower  = mid_load_power;
        } 
        if (i == 3) {
            lcd.print("HIGH:");
            loadCurrent = high_load_current;
            loadPower  = high_load_power;
        } 

        lcd.setCursor(5, i);
        lcd.print(loadCurrent);
        lcd.setCursor(9, i);
        lcd.print("A"); 

        lcd.setCursor(10, i);
        lcd.print("||");

        lcd.setCursor(12, i);
        lcd.print(loadPower);
        lcd.setCursor(19, i);
        lcd.print("W");
    }

    // Serial outputs
    Serial.print("System Voltage: ");
    Serial.println(system_voltage);
    Serial.println("-----------------------");
    // load currents 
    Serial.print("Load Current -> LOW: ["); 
    Serial.print(low_load_current);
    Serial.print("] MID: [");
    Serial.print(mid_load_current);
    Serial.print("] HIGH: [");
    Serial.print(high_load_current);
    Serial.println("]");
    // load powers
    Serial.print("Load Power -> LOW: ["); 
    Serial.print(low_load_power);
    Serial.print("] MID: [");
    Serial.print(mid_load_power);
    Serial.print("] HIGH: [");
    Serial.print(high_load_power);
    Serial.println("]");
    Serial.println("-----------------------");
    // SRNE readings 
    Serial.print("[SRNE] Battery Capacity: ");
    Serial.println(battery_capacity);
    Serial.print("[SRNE] Charging Current: ");
    Serial.println(charging_current);
    Serial.print("[SRNE] Panel Voltage: ");
    Serial.println(panel_voltage);
    Serial.print("[SRNE] Panel Current: ");
    Serial.println(panel_current);
    Serial.print("[SRNE] Charging Power: ");
    Serial.println(charging_power);
    Serial.println("-----------------------");
}