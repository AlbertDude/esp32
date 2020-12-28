#include <Arduino.h>

unsigned long g_start_ms;

void serial_log(String msg) {
    // TODO: do wraparound check
    unsigned long elapsed = millis() - g_start_ms;
    float elapsed_secs = (float)(elapsed)/1000.f;
    Serial.println(String(elapsed_secs, 3) + ": " + msg);
}

#define USE_TICKER (0)

#if USE_TICKER
// USE Ticker to blink LED

#include <Ticker.h>

// T3 aka D15
#define LED_PIN T3

Ticker blinker;
Ticker toggler;
Ticker changer;
float blinkerPace = 0.1;  //seconds
const float togglePeriod = 5; //seconds

void change() {
  blinkerPace = 0.5;
}

void blink() {
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void toggle() {
  static bool isBlinking = false;
  if (isBlinking) {
    blinker.detach();
    isBlinking = false;
  }
  else {
    blinker.attach(blinkerPace, blink);
    isBlinking = true;
  }
  digitalWrite(LED_PIN, LOW);  //make sure LED on on after toggling (pin LOW = led ON)
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  toggler.attach(togglePeriod, toggle);
  changer.once(30, change);
}

void loop() {

}

#else

// USE polled blinker
#include "../../LoopTimer/src/LoopTimer.h"

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

class PolledBlinker
{
public:
    PolledBlinker(uint8_t pin, unsigned interval)
    : m_interval(interval)
    , m_pin(pin)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, OUTPUT); // Set specified pin to be an OUTPUT
        m_prevToggle = 0;
        m_ledState = LOW;
    }
    void loop()
    {
        unsigned long now = millis();
        if((m_prevToggle == 0) || (now >= m_prevToggle + m_interval)){
            m_ledState = (m_ledState == LOW) ? HIGH : LOW;
            digitalWrite(m_pin, m_ledState); //
            serial_log("Toggled LED");
            m_prevToggle = now;
        }
    }
private:
    unsigned m_interval;
    uint8_t m_pin;
    unsigned long m_prevToggle;
    int m_ledState;
};

LoopTimer loopTimer;
PolledBlinker heartbeat(LED_BUILTIN, 1500); // BUILTIN_LED also appears on GPIO2
PolledBlinker extLed(T3, 300);              // Touch3 = GPIO15

// This runs on powerup
// - put your setup code here, to run once:
void setup() {
    Serial.begin(115200); // for serial link back to computer

    g_start_ms = millis();
}

// Then this loop runs forever
// - put your main code here, to run repeatedly:
void loop() {
    loopTimer.loop();     // typically 246930 calls/sec

    heartbeat.loop();
    extLed.loop();
}

#endif

// vim: sw=4:ts=4