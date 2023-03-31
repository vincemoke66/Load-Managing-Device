/* LIBRARIES */
#include <LiquidCrystal_I2C.h>  // for LCD
#include <AltSoftSerial.h>      // for SRNE 
#include <ModbusMaster.h>       // for SRNE
#include "EmonLib.h"            // for SRNE
#include <PZEM004Tv30.h>        // for PZEM
#include <SoftwareSerial.h>     // for PZEM

/* PIN CONFIGURATION */
#define HIGH_RELAY_PIN 2
#define MID_RELAY_PIN 3
#define LOW_RELAY_PIN 4
#define RESET_PIN 5
#define RS485_DE_PIN 6
#define RS485_RE_PIN 7

/* CONSTANTS */
const int LOW_LOAD_BATT_REQ = 65;       // **
const int MID_LOAD_BATT_REQ = 80;       // LOAD BATTERY REQUIREMENTS
const int HIGH_LOAD_BATT_REQ = 100;     // **
const int LOW_LOAD_MAX_POWER = 300;     // **
const int MID_LOAD_MAX_POWER = 600;     // LOAD MAX ALLOWABLE POWER
const int HIGH_LOAD_MAX_POWER = 1000;   // **
const int POWER_READING_INTERVAL = 3;   // power readings and display interval in seconds

/* SRNE VARIABLES */
int srne_battery_capacity = 0;
int srne_charging_current = 0;
int srne_panel_voltage = 0;
int srne_panel_current = 0;
int srne_charging_power = 0;

/* RESET BUTTON VARIABLE */
bool prev_reset_value = false;

// LOAD POWER VARIABLES for PZEM readings 
float voltage;
float current;
float power;
float energy;
float frequency;
float pf;

// TIME OF PREVIOUS LOAD POWER READING
unsigned long present_power_calculation_time = 0;

/* OBJECTS */
LiquidCrystal_I2C lcd(0x27, 20, 4); 
EnergyMonitor clamp; 
AltSoftSerial swSerial;
ModbusMaster node;
ACS712 acs_sensor(ACS712_20A, ACS_PIN);
SoftwareSerial pzemSWSerial(11, 12); // * [11 -> PZEM Tx] | [12 -> PZEM Rx]
PZEM004Tv30 pzem;

/* FOR TESTING */
bool isTesting = true;
int testing_battery_capacity = 100;

void setup() {
    Serial.begin(9600);

    // PZEM init
    pzem = PZEM004Tv30(pzemSWSerial);

    // RS485 init
    pinMode(RS485_DE_PIN, OUTPUT);
    pinMode(RS485_RE_PIN, OUTPUT);
    postTransmission();

    // Modbus communicaiton for SRNE 
    swSerial.begin(9600);
    node.begin(1, swSerial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // sets load relay as output
    pinMode(LOW_RELAY_PIN, OUTPUT);
    pinMode(MID_RELAY_PIN, OUTPUT);
    pinMode(HIGH_RELAY_PIN, OUTPUT);

    // sets reset button as input
    pinMode(RESET_PIN, INPUT_PULLUP);

    // LCD init
    lcd.init();
    lcd.backlight();

    // initial bootup functions 
    readSRNE();
    allowLoadConnection();
    prev_reset_value = digitalRead(RESET_PIN);
}

void loop() {
    // handles reset button on press event 
    if (digitalRead(RESET_PIN) != prev_reset_value) reset();
    
    // read SRNE data
    readSRNE();

    // read load power
    readPZEM();

    // trip off load connection based on load power and present battery level
    tripOffLoadConnection();

    // TODO: add lcd display

    // delay
    delay(POWER_READING_INTERVAL * 1000);
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

void reset() {
    prev_reset_value = digitalRead(RESET_PIN);
    readSRNE();
    allowLoadConnection();
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

void allowLoadConnection() {
    if (srne_battery_capacity >= LOW_LOAD_BATT_REQ) digitalWrite(LOW_RELAY_PIN, LOW); // allows low load connection
    if (srne_battery_capacity >= MID_LOAD_BATT_REQ) digitalWrite(MID_RELAY_PIN, LOW); // allows mid load connection
    if (srne_battery_capacity >= HIGH_LOAD_BATT_REQ) digitalWrite(HIGH_RELAY_PIN, LOW); // allows high load connection
}

void readPZEM() {
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    energy = pzem.energy();
    frequency = pzem.frequency();
    pf = pzem.pf(); // power factor
}

void tripOffLoadConnection() {
    // trips off load connection based on battery level
    if (srne_battery_capacity < HIGH_LOAD_BATT_REQ) digitalWrite(HIGH_RELAY_PIN, HIGH); // allows high load connection
    if (srne_battery_capacity < MID_LOAD_BATT_REQ) digitalWrite(MID_RELAY_PIN, HIGH); // allows mid load connection
    if (srne_battery_capacity < LOW_LOAD_BATT_REQ) digitalWrite(LOW_RELAY_PIN, HIGH); // allows low load connection

    // trips off load connection based on load power 
    if (power >= LOW_LOAD_MAX_POWER) digitalWrite(LOW_RELAY_PIN, HIGH);     // trips off low load connection
    if (power >= MID_LOAD_MAX_POWER) digitalWrite(MID_RELAY_PIN, HIGH);     // trips off mid load connection
    if (power >= HIGH_LOAD_MAX_POWER) digitalWrite(HIGH_RELAY_PIN, HIGH);   // trips off high load connection
}