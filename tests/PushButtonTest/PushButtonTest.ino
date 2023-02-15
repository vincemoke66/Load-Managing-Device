unsigned long start_time = 0;
unsigned long interval = 500;
unsigned long current_time = 0;
void setup() {
    Serial.begin(9600);
    pinMode(5, INPUT);
}

void loop() {
    start_time = millis();
    if (start_time > current_time + interval) {
        Serial.println(digitalRead(5));
        current_time = start_time;
    }
}