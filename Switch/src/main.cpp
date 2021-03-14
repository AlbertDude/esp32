// The circuit:
// - button-switch attached from pin to +3V3
//  - this sets pin HIGH when button-switch is closed
// - 10 kilohm resistor attached from pin to ground
//  - this pulls pin LOW when button-switch is open
/*
           ╔═════════════════════════════════════╗      
           ║            ESP-WROOM-32             ║      
           ║               Devkit                ║      
           ║                                     ║      
           ║EN /                         MOSI/D23║      
           ║VP /A0                        SCL/D22║      
           ║VN /A3                         TX/TX0║      
           ║D34/A6                         RX/RX0║      
           ║D35/A7                        SDA/D21║      
           ║D32/A4,T9                    MISO/D19║      
           ║D33/A5,T8                     SCK/D18║  3V3    
           ║D25/A18,DAC1                   SS/ D5║   ○      
           ║D26/A19,DAC2                     /TX2║   │   
           ║D27/A17,T7                       /RX2║   ⁄   
           ║D14/A16,T6                 T0,A10/ D4║───┤    
           ║D12/A15,T5     LED_BUILTIN,T2,A12/ D2║   R
           ║D13/A14,T4                 T3,A13/D15║   │
           ║GND/                             /GND║   ▽
           ║VIN/                             /3V3║
           ║                                     ║      
           ║   EN           μUSB           BOOT  ║      
           ╚═════════════════════════════════════╝      
*/

#include <Arduino.h>
#include "Switch.h"
#include "../../SerialLog/include/SerialLog.h"

//-----------------------------------------------------------------

Switch button_switch(T0); // Touch0 = GPIO04

// This runs on powerup
// - put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    pinMode(LED_BUILTIN, OUTPUT); // LED will follow switch state

    SerialLog::Log(__FILE__);
}

// Then this loop runs forever
// - put your main code here, to run repeatedly:
void loop() {
    button_switch.Loop();

    if(button_switch.IsHigh())
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
}

// vim: sw=4:ts=4
