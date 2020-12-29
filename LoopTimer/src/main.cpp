#include <Arduino.h>
#include "LoopTimer.h"

#include "../../SerialLog/include/SerialLog.h"

LoopTimer loopTimer;

// This runs on powerup
// - put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    SerialLog::log(__FILE__);
}

// Then this loop runs forever
// - put your main code here, to run repeatedly:
void loop() {
    loopTimer.loop();     // typically 695482 calls/sec
}

// vim: sw=4:ts=4
