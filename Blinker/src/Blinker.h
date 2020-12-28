#pragma once

#include "../../Ticker/src/Ticker.h"

void serial_log(String msg);

// LED Blinker class
// - toggles specified GPIO pin at specified interval
// - calling code needs to call ::loop() periodically at rate faster than the specified interval
class Blinker
{
public:
    Blinker(uint8_t pin, unsigned interval_ms)
    : m_interval_ms(interval_ms)
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
        if((m_prevToggle == 0) || (now >= m_prevToggle + m_interval_ms)){
            m_ledState = (m_ledState == LOW) ? HIGH : LOW;
            digitalWrite(m_pin, m_ledState); //
            serial_log("Toggled LED");
            m_prevToggle = now;
        }
    }

private:
    unsigned m_interval_ms;
    uint8_t m_pin;
    unsigned long m_prevToggle;
    int m_ledState;
};


// LED Blinker class, implemented with Ticker
// - toggles specified GPIO pin at specified interval
// - calling code just needs to instantiate class instance which will setup Ticker fcn to periodically
// update the pin-output state
class BlinkerT
{
public:
    BlinkerT(uint8_t pin, unsigned interval_ms)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, OUTPUT); // Set specified pin to be an OUTPUT
        m_ticker.attach_ms<uint8_t>(interval_ms, BlinkerT::loop, pin);
    }

    ~BlinkerT()
    {
        m_ticker.detach();
    }

private:
    static void loop(uint8_t pin)
    {
        digitalWrite(pin, !digitalRead(pin));
    }
    Ticker m_ticker;
};
// vim: sw=4:ts=4
