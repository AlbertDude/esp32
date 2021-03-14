//-----------------------------------------------------------------
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
      ║D33/A5,T8                     SCK/D18║      
      ║D25/A18,DAC1                   SS/ D5║      
      ║D26/A19,DAC2                     /TX2║      
      ║D27/A17,T7                       /RX2║      
      ║D14/A16,T6                 T0,A10/ D4║      
      ║D12/A15,T5     LED_BUILTIN,T2,A12/ D2║─ LED0          ↗↗
      ║D13/A14,T4                 T3,A13/D15║────────R───────▶──┐           
      ║GND/                             /GND║                   │
      ║VIN/                             /3V3║                   ▽ Gnd
      ║                                     ║      
      ║   EN           μUSB           BOOT  ║      
      ╚═════════════════════════════════════╝      

*/

#include <Arduino.h>
#include "Blinker.h"
#include "../../LoopTimer/include/LoopTimer.h"
#include "../../SerialLog/include/SerialLog.h"

LoopTimer loop_timer;

#define USE_LOOPED (1)
#if USE_LOOPED
#define BLINKER BlinkerL
#else
#define BLINKER Blinker
#endif

BLINKER builtin(LED_BUILTIN, 1500);     // BUILTIN_LED also appears on GPIO2
BLINKER external(T3, 300);              // Touch3 = GPIO15

// This runs on powerup
// -  put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    SerialLog::Log(__FILE__);
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() {
    loop_timer.Loop();     // typically: 246930 calls/sec or 695400 calls/sec using Ticker

#if USE_LOOPED
    builtin.Loop();
    external.Loop();
#endif
}

// vim: sw=4:ts=4
