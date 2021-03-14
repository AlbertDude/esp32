#pragma once

#include "../../SerialLog/include/SerialLog.h"
#include "../../Ticker/include/Ticker.h"

// LED Blinker class, implemented with Ticker
// - toggles specified GPIO pin at specified interval
// - calling code just needs to instantiate class instance which will setup Ticker fcn to periodically
// update the pin-output state
class Blinker
{
public:
    Blinker(uint8_t pin, unsigned interval_millis)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, OUTPUT); // Set specified pin to be an OUTPUT
        ticker_.attach_ms<uint8_t>(interval_millis, Blinker::_Loop, pin);
    }

    ~Blinker()
    {
        ticker_.detach();
    }

private:
    static void _Loop(uint8_t pin)
    {
        digitalWrite(pin, !digitalRead(pin));
    }
    Ticker ticker_;
};


// LED Blinker class
// - toggles specified GPIO pin at specified interval
// - calling code needs to call ::Loop() periodically at rate faster than the specified interval
class BlinkerL
{
public:
    BlinkerL(uint8_t pin, unsigned interval_millis)
        : interval_millis_(interval_millis)
        , pin_(pin)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, OUTPUT); // Set specified pin to be an OUTPUT
        prev_toggle_millis_ = 0;
        output_state_ = LOW;
    }

    void Loop()
    {
        unsigned long now = millis();
        if( (prev_toggle_millis_ == 0) || (now >= prev_toggle_millis_ + interval_millis_) )
        {
            output_state_ = (output_state_ == LOW) ? HIGH : LOW;
            digitalWrite(pin_, output_state_); //
            SerialLog::Log("Toggled LED");
            prev_toggle_millis_ = now;
        }
    }

private:
    unsigned interval_millis_;
    uint8_t pin_;
    unsigned long prev_toggle_millis_;
    int output_state_;
};


// vim: sw=4:ts=4
