#define ACS_PIN A0

void setup() {
    Serial.begin(9600); 
    pinMode(ACS_PIN, INPUT);
}
 
void loop() {
    unsigned int x = 0;
    float AcsValue = 0.0, Samples = 0.0, AvgAcs = 0.0, AcsValueF = 0.0;
    
    for (x = 0; x < 150; x++) { //Get 150 samples
        AcsValue = analogRead(ACS_PIN);     //Read current sensor values
        Samples = Samples + AcsValue;  //Add samples together
        delay (3); // let ADC settle before following sample 3ms
    }
    AvgAcs = Samples / 150.0; //Taking Average of Samples
    AcsValueF = (2.5 - (AvgAcs * (5.0 / 1024.0)) ) / 0.1;
    
    Serial.println(AcsValueF);//Print the read current on Serial monitor
    delay(50);
}