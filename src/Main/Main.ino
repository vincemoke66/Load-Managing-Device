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
// LOAD BATTERY REQUIREMENTS AND LIMITS
const int LOW_LOAD_BATT_REQ = 65;
const int LOW_LOAD_BATT_LIMIT = 50;
//
const int MID_LOAD_BATT_REQ = 80;
const int MID_LOAD_BATT_LIMIT = 65;
//
const int HIGH_LOAD_BATT_REQ = 95;
const int HIGH_LOAD_BATT_LIMIT = 80;

// LOAD MAX ALLOWABLE POWER
const int LOW_LOAD_MAX_POWER = 300;
const int MID_LOAD_MAX_POWER = 600;
const int HIGH_LOAD_MAX_POWER = 1000;
const int DATA_READING_INTERVAL = 3;

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

// TIME OF PREVIOUS DATA READING
unsigned long previous_data_reading = 0;

/* OBJECTS */
LiquidCrystal_I2C lcd(0x27, 20, 4); 
EnergyMonitor clamp; 
AltSoftSerial swSerial;
ModbusMaster node;
SoftwareSerial pzemSWSerial(11, 12); // * [11 -> PZEM Tx] | [12 -> PZEM Rx]
PZEM004Tv30 pzem;

/* FOR TESTING */
bool isTesting = true;
int testing_battery_capacity = 100;

/* LCD CUSTOM CHARACTERS */
byte customBattFull[] = { 0x0E, 0x0A, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };
byte customBatt80AndUp[] = { 0x0E, 0x0A, 0x11, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };
byte customBatt60AndUp[] = { 0x0E, 0x0A, 0x11, 0x11, 0x1F, 0x1F, 0x1F, 0x1F };
byte customBatt50AndBelow[] = { 0x0E, 0x0A, 0x15, 0x15, 0x11, 0x15, 0x11, 0x1F };
byte customLightning[] = { 0x00, 0x02, 0x04, 0x0C, 0x1F, 0x06, 0x04, 0x08 };
byte customPlug[] = { 0x0A, 0x0A, 0x1F, 0x11, 0x11, 0x0E, 0x04, 0x04 };
byte customLow[] = { 0x0E, 0x1F, 0x1B, 0x1B, 0x0A, 0x11, 0x1B, 0x0E };
byte customWarning[] = { 0x0E, 0x11, 0x15, 0x15, 0x11, 0x15, 0x11, 0x0E };

/* BOOLEANS */
int lowLoadStatus = 0;
int midLoadStatus = 0;
int highLoadStatus = 0;

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

    // LCD create custom characters
    lcd.createChar(0, customBattFull);
    lcd.createChar(1, customBattFull);
    lcd.createChar(2, customBatt80AndUp);
    lcd.createChar(3, customBatt60AndUp);
    lcd.createChar(4, customBatt50AndBelow);
    lcd.createChar(5, customLightning);
    lcd.createChar(6, customPlug);
    lcd.createChar(7, customLow);
    lcd.createChar(8, customWarning);

    // initial bootup functions 
    readSRNE();
    allowLoadConnection(false);
    prev_reset_value = digitalRead(RESET_PIN);
}

void loop() {
    // handles reset button on press event 
    if (digitalRead(RESET_PIN) != prev_reset_value) reset();
    
    // read SRNE data
    readSRNE();

    // read load power
    readPZEM();

    // allow load connection but check if load has not overloaded
    allowLoadConnection(true);

    // trip off load connection based on load power and present battery level
    tripOffLoadConnection();

    // data display delay
    if (millis() - previous_data_reading > (DATA_READING_INTERVAL * 1000)) {
        previous_data_reading = millis();
        displayData();
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

void reset() {
    prev_reset_value = digitalRead(RESET_PIN);
    readSRNE();
    allowLoadConnection(false);
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

void allowLoadConnection(bool shouldCheckOverloaded) {
    bool lowHasChecked = true;
    bool midHasChecked = true;
    bool highHasChecked = true;

    if (shouldCheckOverloaded) {
        lowHasChecked = lowLoadStatus == 1;
        midHasChecked = midLoadStatus == 1;
        highHasChecked = highLoadStatus == 1;
    }

    if (lowHasChecked && srne_battery_capacity >= LOW_LOAD_BATT_REQ) {
        digitalWrite(LOW_RELAY_PIN, LOW); // allows low load connection
        lowLoadStatus = 2; // sets low load status as sufficient
    }
    if (midHasChecked && srne_battery_capacity >= MID_LOAD_BATT_REQ) {
        digitalWrite(MID_RELAY_PIN, LOW); // allows mid load connection
        midLoadStatus = 2; // sets mid load status as sufficient
    }
    if (highHasChecked && srne_battery_capacity >= HIGH_LOAD_BATT_REQ) {
        digitalWrite(HIGH_RELAY_PIN, LOW); // allows high load connection
        highLoadStatus = 2; // sets high load status as sufficient
    }
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
    if (srne_battery_capacity < HIGH_LOAD_BATT_LIMIT) {
        digitalWrite(HIGH_RELAY_PIN, HIGH); // trips off high load connection
        highLoadStatus = 1; // sets high load status as insufficient 
    }
    if (srne_battery_capacity < MID_LOAD_BATT_LIMIT) {
        digitalWrite(MID_RELAY_PIN, HIGH); // trips mid load connection
        midLoadStatus = 1; // sets mid load status as insufficient 
    }
    if (srne_battery_capacity < LOW_LOAD_BATT_LIMIT) {
        digitalWrite(LOW_RELAY_PIN, HIGH); // allows low load connection
        lowLoadStatus = 1; // sets low load status as insufficient 
    }

    // trips off load connection based on load power 
    if (power > HIGH_LOAD_MAX_POWER) {
        digitalWrite(HIGH_RELAY_PIN, HIGH);   // trips off high load connection
        highLoadStatus = 0; // sets high load status as overloaded
    }
    if (power > MID_LOAD_MAX_POWER && (highLoadStatus == 0 || highLoadStatus == 1)) {
        digitalWrite(MID_RELAY_PIN, HIGH);     // trips off mid load connection
        midLoadStatus = 0; // sets mid load status as overloaded
    }
    if (power > LOW_LOAD_MAX_POWER && (midLoadStatus == 0 || midLoadStatus == 1)) {
        digitalWrite(LOW_RELAY_PIN, HIGH);     // trips off low load connection
        lowLoadStatus = 0; // sets low load status as overloaded
    }
}

void displayData() {
    lcd.clear();

    /******** ROW 0  **********/
    // * BATTERY LEVEL
    lcd.setCursor(0, 1);
    if (srne_battery_capacity == 100) lcd.write(0); // **
    if (srne_battery_capacity >= 80) lcd.write(1);  // battery 
    if (srne_battery_capacity >= 60) lcd.write(2);  // symbol
    if (srne_battery_capacity <= 50) lcd.write(3);  // **

    lcd.setCursor(0, 3);                // **
    lcd.print(srne_battery_capacity);   // battery level value
    lcd.print(" % | ");                 // **

    lcd.setCursor(0, 11);   // **
    lcd.write(4);           // lightning symbol
    lcd.setCursor(0, 13);   // ** 
    lcd.print(power);       // power
    lcd.setCursor(0, 18);   // value
    lcd.print("W");         // **
    /************************/

    /******** ROW 1  **********/
    lcd.setCursor(1, 0);
    lcd.print("LOW: ");
    if (lowLoadStatus == 0) {
        lcd.setCursor(1, 6);
        lcd.print("OVERLOADED  ");
        lcd.write(6);
    }
    if (lowLoadStatus == 1) {
        lcd.setCursor(1, 5);
        lcd.print("INSUFFICIENT ");
        lcd.write(7);
    }
    if (lowLoadStatus == 2) {
        lcd.setCursor(1, 6);
        lcd.print("SUFFICIENT  ");
        lcd.write(5);
    }
    /************************/

    /******** ROW 2  **********/
    lcd.setCursor(2, 0);
    lcd.print("MID: ");
    if (midLoadStatus == 0) {
        lcd.setCursor(2, 6);
        lcd.print("OVERLOADED  ");
        lcd.write(6);
    }
    if (midLoadStatus == 1) {
        lcd.setCursor(2, 5);
        lcd.print("INSUFFICIENT ");
        lcd.write(7);
    }
    if (midLoadStatus == 2) {
        lcd.setCursor(2, 6);
        lcd.print("SUFFICIENT  ");
        lcd.write(5);
    }
    /************************/

    /******** ROW 3  **********/
    lcd.setCursor(3, 0);
    lcd.print("HIG: ");
    if (highLoadStatus == 0) {
        lcd.setCursor(3, 6);
        lcd.print("OVERLOADED  ");
        lcd.write(6);
    }
    if (highLoadStatus == 1) {
        lcd.setCursor(3, 5);
        lcd.print("INSUFFICIENT ");
        lcd.write(7);
    }
    if (highLoadStatus == 2) {
        lcd.setCursor(3, 6);
        lcd.print("SUFFICIENT  ");
        lcd.write(5);
    }
    /************************/

    /* SERIAL PRINT */
    Serial.print("Battery Level: ");
    Serial.print(srne_battery_capacity);
    Serial.print("% | ");
    Serial.print("Load Power: ");
    Serial.print(power);
    Serial.println("W");

    Serial.print("LOW Load Status: ");
    Serial.println(lowLoadStatus);
    Serial.print("MID Load Status: ");
    Serial.println(midLoadStatus);
    Serial.print("HIGH Load Status: ");
    Serial.println(highLoadStatus);

    Serial.println("- - - - - - - - - - -");    // separator
    /***************/
}