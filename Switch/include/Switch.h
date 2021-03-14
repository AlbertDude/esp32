
// Switch reading helper class
// - Provides debounced high/low readings for interfacing with electrical switches
// - Should be fine with normally-open/closed, momentary or latching.
// - While switches are normally classified as being open or closed, from the POV of the input pin,
// won't be able to infer the switch position -- only whether the switch position and the specific
// circuit results in a high or low input state.
//
// Switch state machine:
//
//           UNDEFINED
//               ^
//               v
//        <->  RISING  ->
//   LOW                  HIGH
//        <-  FALLING <->
//
//               ^
//               v
//           UNDEFINED
//
// During the RISING & FALLING states, the switch is in a debounce phase and may advance to the
// next state or retreat to the previous.

#pragma once

#include "../../SerialLog/include/SerialLog.h"

class Switch
{
public:

    Switch(uint8_t pin)
    : pin_(pin)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, INPUT); // Set specified pin to be an INPUT
    }

    bool IsLow()
    {
        return (state_ == kLow) || (prev_state_ == kLow);
    }

    bool IsHigh()
    {
        return (state_ == kHigh) || (prev_state_ == kHigh);
    }

    void Loop()
    {
        // read the state of the switch into a local variable:
        int reading = digitalRead(pin_);

        switch(state_)
        {
// TODO: seems I got Rising & Falling reversed, (still works but confusing to read...)
// XXX
            case kUndefined:
                if( reading == HIGH )
                {
                    state_ = kRising;
                }
                else
                {
                    assert( reading == LOW );
                    state_ = kFalling;
                }
                prev_state_ = kUndefined;
                debounce_start_ = millis();
                break;
            case kLow:
                if( reading == HIGH )
                {
                    state_ = kRising;
                    debounce_start_ = millis();
                }
                prev_state_ = kLow;
                break;
            case kRising:
                if( reading == LOW )
                {
                    state_ = prev_state_;
                }
                else
                {
                    if( millis() > debounce_start_ + kDebounceDelay )
                    {
                        state_ = kHigh;
                        SerialLog::Log("Transiting to HIGH state");
                    }
                }
                break;
            case kHigh:
                if( reading == LOW )
                {
                    state_ = kFalling;
                    debounce_start_ = millis();
                }
                prev_state_ = kHigh;
                break;
            case kFalling:
                if( reading == HIGH )
                {
                    state_ = prev_state_;
                }
                else
                {
                    if( millis() > debounce_start_ + kDebounceDelay )
                    {
                        state_ = kLow;
                        SerialLog::Log("Transiting to LOW state");
                    }
                }
                break;
            default:
                assert(false);  // Bad State
        };

    }

private:
    enum State
    {
        kUndefined, // Used at startup when switch state is unknown
        kLow,
        kFalling,
        kHigh,
        kRising,
    };

    uint8_t pin_;
    State state_ = kUndefined;
    State prev_state_ = kUndefined;
    unsigned long debounce_start_;
    static const unsigned long kDebounceDelay = 50;    // in millis
};

// vim: sw=4:ts=4
