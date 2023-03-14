#include <LiquidCrystal_I2C.h>
#include <AltSoftSerial.h>
#include <ModbusMaster.h>
#include "EmonLib.h"
#include "ACS712.h"

/* PIN CONFIGURATION */
#define LOW_RELAY_PIN 4
#define MID_RELAY_PIN 3
#define HIGH_RELAY_PIN 2

#define ACS_PIN A0
#define CLAMP_PIN A1
#define VOLTAGE_SENSOR A2

#define RESET_PIN 5

#define RS485_DE_PIN 6
#define RS485_RE_PIN 7

/* CONSTANTS */
const int LOW_LOAD_BATT_MINIMUM = 65;   // **
const int MID_LOAD_BATT_MINIMUM = 80;   // LOAD BATTERY REQUIREMENTS
const int HIGH_LOAD_BATT_MINIMUM = 100; // **

const int LOW_LOAD_MAX_POWER = 300;     // **
const int MID_LOAD_MAX_POWER = 600;     // LOAD MAX ALLOWABLE POWER
const int HIGH_LOAD_MAX_POWER = 1000;   // **

const int POWER_CALCULATION_INTERVAL = 5000;

const double current_calibration = 0.00714;

/* SRNE VARIABLES */
int srne_battery_capacity = 0;
int srne_charging_current = 0;
int srne_panel_voltage = 0;
int srne_panel_current = 0;
int srne_charging_power = 0;

/* CURRENT AND VOLTAGE */
float acs_raw_value = 0;
float acs_final_value = 0;
float clamp_value = 0;
float raw_voltage = 0;
float system_voltage = 0;
float average_final_current = 0;

/* POWER VALUES */
float acs_load_power = 0;
float clamp_load_power = 0;

/* RESET BUTTON VALUES */
int prev_reset_value = 0;
int curr_reset_value = 0;

unsigned long current_power_calculation_time = 0;

/* BOOLEANS */
bool batteryCanSupplyLowLoad = false;   // ***
bool batteryCanSupplyMidLoad = false;   // battery-based booleans
bool batteryCanSupplyHighLoad = false;  // ***
bool lowLoadReachedMaxPower = false;    // ***
bool midLoadReachedMaxPower = false;    // load power-based booleans
bool highLoadReachedMaxPower = false;   // ***

/* OBJECTS */
LiquidCrystal_I2C lcd(0x27, 20, 4); 
EnergyMonitor clamp; 
AltSoftSerial swSerial;
ModbusMaster node;
ACS712 acs_sensor(ACS712_20A, ACS_PIN);

/* FOR TESTING WITH CUSTOM BATTERY CAPACITY */
bool isTesting = true;
int testing_battery_capacity = 100;

void setup() {
    Serial.begin(9600);

    // RS485 init
    pinMode(RS485_DE_PIN, OUTPUT);
    pinMode(RS485_RE_PIN, OUTPUT);
    postTransmission();

    // Modbus communicaiton for SRNE 
    swSerial.begin(9600);
    node.begin(1, swSerial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // assigns current sensors to emon objects
    clamp.current(CLAMP_PIN, 111.1);

    // sets load relay as output
    pinMode(LOW_RELAY_PIN, OUTPUT);
    pinMode(MID_RELAY_PIN, OUTPUT);
    pinMode(HIGH_RELAY_PIN, OUTPUT);

    // sets reset button as input
    pinMode(RESET_PIN, INPUT_PULLUP);

    // ACS Sensor calibration
    acs_sensor.calibrate();

    // LCD init
    lcd.init();
    lcd.backlight();
    
    /* FOR TESTING */
    digitalWrite(LOW_RELAY_PIN, LOW);

    // initial process 
    prev_reset_value = digitalRead(RESET_PIN);
    readSRNE();
    analyzeAllowableLoad();
    supplyAllowedLoads();
}

void loop() {
    // handles reset event when user pressed reset button
    curr_reset_value = digitalRead(RESET_PIN);
    if (curr_reset_value != prev_reset_value) {
        curr_reset_value = prev_reset_value;
        reset();
    }

    /* POWER CONTROL ON BATTERY CAPACITY*/
    // read SRNE values
    readSRNE();
    // analyze SRNE values base on battery capacity
    analyzeAllowableLoad();
    // analyze which load to trip off 
    analyzeLoadToTripOff();
    /***********************************/

    /* POWER CONTROL ON LOAD POWER */
    // read ACS value
    readACS();
    // read clamp sensor value
    readClampCurrent();
    // read voltage
    readVoltage();
    // calculate load power with each current values
    calculateLoadPowerConsumption();    
    // analyze which load to trip off 
    analyzeLoadToTripOff();
    /***********************************/

    /* SERPARATOR */
    Serial.println("- - - - - -");

    delay(2000);
}

void reset() {
    readSRNE();
    analyzeAllowableLoad();
    supplyAllowedLoads();
}

void readSRNE() {
    if (isTesting) {
        srne_battery_capacity = testing_battery_capacity;

        /* Serial output */
        Serial.print("Battery Capacity: ");
        Serial.println(srne_battery_capacity);

        return;
    }

    uint8_t result;

    // PARSED DATA
    result = node.readHoldingRegisters(0x0100, 9);
    if (result == node.ku8MBSuccess) {
        // reading of desired registers 
        srne_battery_capacity = node.getResponseBuffer(0x00);
        srne_charging_current = node.getResponseBuffer(0x02) * 0.01f;
        srne_panel_voltage = node.getResponseBuffer(0x07) * 0.1f;
        srne_panel_current = node.getResponseBuffer(0x08) * 0.01f;
        srne_charging_power = node.getResponseBuffer(0x09);
    }

    /* Serial output */
    Serial.print("Battery Capacity: ");
    Serial.println(srne_battery_capacity);
}

// determines which line to be allowed based on battery capacity 
void analyzeAllowableLoad() {
    if (srne_battery_capacity >= LOW_LOAD_BATT_MINIMUM) {
        batteryCanSupplyLowLoad = true;
        lowLoadReachedMaxPower = false;
    }
    if (srne_battery_capacity >= MID_LOAD_BATT_MINIMUM) {
        batteryCanSupplyMidLoad = true;
        midLoadReachedMaxPower = false;
    }
    if (srne_battery_capacity >= HIGH_LOAD_BATT_MINIMUM) {
        batteryCanSupplyHighLoad = true;
        highLoadReachedMaxPower = false;
    }
}

void supplyAllowedLoads() {
    if (batteryCanSupplyLowLoad) {
        digitalWrite(LOW_RELAY_PIN, HIGH);
        Serial.println("LOW Load Connection ALLOWED!");
    }
    if (batteryCanSupplyMidLoad) {
        digitalWrite(MID_RELAY_PIN, HIGH);
        Serial.println("MID Load Connection ALLOWED!");
    }
    if (batteryCanSupplyHighLoad) {
        digitalWrite(HIGH_RELAY_PIN, HIGH);
        Serial.println("HIGH Load Connection ALLOWED!");
    }
}

void readACS() {
    acs_final_value = acs_sensor.getCurrentAC();

    //ignoring the value below 0.09
    // if (acs_final_value < 0.09) acs_final_value = 0;

    delay(300);

    // unsigned int x = 0;

    // float acs_samples = 0.0, acs_avg = 0.0, samples = 150.0;
    
    // // collects 150 samples
    // for (x = 0; x < samples; x++) { 
    //     acs_raw_value = analogRead(ACS_PIN);
    //     acs_samples += acs_raw_value;  
        
    //     delay (3); // short break
    // }

    // // calculating average 
    // acs_avg = acs_samples / samples; 
    
    // acs_final_value = (2.5 - (acs_avg * (5.0 / 1024.0)) ) / 0.1;
    
    // serial print
    // Serial.print("ACS Raw: "); 
    // Serial.print(acs_raw_value); 
    Serial.print("ACS Final: "); 
    Serial.println(acs_final_value); 
}

void readClampCurrent() {
    clamp_value = clamp.calcIrms(1480) * current_calibration; 

    // serial print
    Serial.print("Clamp value: "); 
    Serial.println(clamp_value); 
}

void readVoltage() {
    raw_voltage = analogRead(VOLTAGE_SENSOR);
    // system_voltage = ( raw_voltage / 1023.0 ) * 5.0;
    system_voltage = raw_voltage;

    // serial print
    Serial.print("System Voltage: "); 
    Serial.println(system_voltage); 
}

void calculateLoadPowerConsumption() {
    if (millis() - current_power_calculation_time > POWER_CALCULATION_INTERVAL) return;

    current_power_calculation_time = millis();

    acs_load_power = acs_final_value * system_voltage;
    clamp_load_power = clamp_value * system_voltage;

    // serial print
    Serial.print("ACS Load Power: "); 
    Serial.print(acs_load_power); 
    Serial.print("\tClamp Load Power: "); 
    Serial.println(clamp_load_power); 
}

// determines which load to trip off based on load power consumption
void analyzeLoadToTripOff() {

    if (acs_load_power > LOW_LOAD_MAX_POWER || clamp_load_power > LOW_LOAD_MAX_POWER) lowLoadReachedMaxPower = true;
    if (acs_load_power > MID_LOAD_MAX_POWER || clamp_load_power > MID_LOAD_MAX_POWER) midLoadReachedMaxPower = true;
    if (acs_load_power > HIGH_LOAD_MAX_POWER || clamp_load_power > HIGH_LOAD_MAX_POWER) highLoadReachedMaxPower = true;

    if (lowLoadReachedMaxPower || !batteryCanSupplyHighLoad) {
        digitalWrite(LOW_RELAY_PIN, LOW);
        Serial.println("LOW Load Connection TRIPPED OFF!");
    }
    if (midLoadReachedMaxPower || !batteryCanSupplyMidLoad) {
        digitalWrite(MID_RELAY_PIN, LOW);
        Serial.println("MID Load Connection TRIPPED OFF!");
    }
    if (highLoadReachedMaxPower || !batteryCanSupplyHighLoad) {
        digitalWrite(HIGH_RELAY_PIN, LOW);
        Serial.println("HIGH Load Connection TRIPPED OFF!");
    }
}

// SRNE Controller functions
void preTransmission() {
    digitalWrite(RS485_RE_PIN, 1);
    digitalWrite(RS485_DE_PIN, 1);
}

void postTransmission() {
    digitalWrite(RS485_RE_PIN, 0);
    digitalWrite(RS485_DE_PIN, 0);
}