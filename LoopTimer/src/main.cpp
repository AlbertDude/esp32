#include <Arduino.h>
#include "LoopTimer.h"

unsigned long g_start_ms;

void serial_log(String msg) {
    // TODO: do wraparound check
    unsigned long elapsed = millis() - g_start_ms;
    float elapsed_secs = (float)(elapsed)/1000.f;
    Serial.println(String(elapsed_secs, 3) + ": " + msg);
}

//-----------------------------------------------------------------

LoopTimer loopTimer;

// This runs on powerup
// - put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    g_start_ms = millis();
}

// Then this loop runs forever
// - put your main code here, to run repeatedly:
void loop() {
    loopTimer.loop();     // typically 695482 calls/sec
}

// vim: sw=4:ts=4
