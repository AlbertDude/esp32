#include <Arduino.h>
#include "../../LoopTimer/src/LoopTimer.h"
#include "Blinker.h"

unsigned long g_start_ms;

void serial_log(String msg) {
    // TODO: do wraparound check
    unsigned long elapsed = millis() - g_start_ms;
    float elapsed_secs = (float)(elapsed)/1000.f;
    Serial.println(String(elapsed_secs, 3) + ": " + msg);
}

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

LoopTimer loopTimer;

#define USE_TICKER (1)

#if USE_TICKER
BlinkerT heartbeat(LED_BUILTIN, 1500); // BUILTIN_LED also appears on GPIO2
BlinkerT extLed(T3, 300);              // Touch3 = GPIO15
#else
Blinker heartbeat(LED_BUILTIN, 1500); // BUILTIN_LED also appears on GPIO2
Blinker extLed(T3, 300);              // Touch3 = GPIO15
#endif

// This runs on powerup
// -  put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer
    g_start_ms = millis();
}

// Then this loop runs repeatedly forever
// - put your main code here, to run repeatedly:
void loop() {
    loopTimer.loop();     // typically: 246930 calls/sec or 695400 calls/sec using Ticker

#if !USE_TICKER
    heartbeat.loop();
    extLed.loop();
#endif
}

// vim: sw=4:ts=4
