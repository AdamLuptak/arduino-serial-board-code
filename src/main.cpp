#include "Arduino.h"
#include "ArduinoJson.h"

String requestFromClient = "";
boolean isRequestCompleted = false;
int volatile power = 0;
const long measurementInterval = 1000;
long preMillisMeasurements = 0;
long maxPowerInterval = 400;

void handleRequest();

volatile int calculatePowerFromPercent(int32_t tempPower);

void setupTimers();

void setup() {
    Serial.begin(9600);
    requestFromClient.reserve(200);
    Serial.println("Starting arduino");
    DDRB = B11100000;  // sets pins 11, 12, 13 as outputs
    setupTimers();
}

void setupTimers() {// initialize Timer1
    noInterrupts(); // disable all interrupts
    //set timer1 interrupt at 1Hz
    TCCR1A = 0;// set entire TCCR1A register to 0
    TCCR1B = 0;// same for TCCR1B
    TCNT1 = 0;//initialize counter value to 0
    // set compare match register for 1hz increments
    OCR1A = 14.625;// = (16*10^6) / (1000*1024) - 1 (must be <65536) every millisecond
    // turn on CTC mode
    TCCR1B |= (1 << WGM12);
    // Set CS10 and CS12 bits for 1024 prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);
    // enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    interrupts(); // enable all interrupts
}

int volatile timeCounter = 0;
ISR(TIMER1_COMPA_vect) {
    if (power > 0 && timeCounter <= power) {
        PORTB = B11100000;
    } else {
        PORTB = B00000000;
    }
    timeCounter = timeCounter == maxPowerInterval ? 0 : timeCounter + 1;
}

void loop() {
    handleRequest();

    long actualMillis = millis();
    if (actualMillis - preMillisMeasurements >= measurementInterval) {
        Serial.println("Starting measurement");
        int temp1 = analogRead(0);
        int temp2 = analogRead(1);
        int temp3 = analogRead(2);
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &temperaturesJson = jsonBuffer.createObject();
        temperaturesJson.set("temp1", temp1);
        temperaturesJson.set("temp2", temp2);
        temperaturesJson.set("temp3", temp3);
        temperaturesJson.set("powerInterval", maxPowerInterval);
        temperaturesJson.set("power", power);
        temperaturesJson.prettyPrintTo(Serial);
        preMillisMeasurements = actualMillis;
    }
}

void handleRequest() {
    if (isRequestCompleted) {
        Serial.println(requestFromClient);

        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &requestFromClientJson = jsonBuffer.parseObject(requestFromClient);

        if (requestFromClientJson.success()) {
            int32_t tempPower = requestFromClientJson["power"];
            if (tempPower <= 100 && tempPower >= 0) {
                Serial.print("Power from client change to: ");
                power = calculatePowerFromPercent(tempPower);
            }
        } else {
            Serial.print("Power from client will not be change can't parse request ");
        }

        requestFromClient = "";
        Serial.println(power);
        isRequestCompleted = false;
    }
}

volatile int calculatePowerFromPercent(int32_t tempPower) { return (volatile int) ((maxPowerInterval * tempPower) / 100); }

void serialEvent() {
    while (Serial.available()) {
        char inChar = (char) Serial.read();
        requestFromClient += inChar;

        if (inChar == '\n') {
            isRequestCompleted = true;
        }
    }
}