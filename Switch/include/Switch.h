
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
    : m_pin(pin)
    {
        // TODO: check that pin is valid GPIO
        pinMode(pin, INPUT); // Set specified pin to be an INPUT
    }

    bool isLow()
    {
        return (m_state == State_Low) || (m_prevState == State_Low);
    }

    bool isHigh()
    {
        return (m_state == State_High) || (m_prevState == State_High);
    }

    void loop()
    {
        // read the state of the switch into a local variable:
        int reading = digitalRead(m_pin);

        switch(m_state)
        {
            case State_Undefined:
                if( reading == HIGH )
                {
                    m_state = State_Falling;
                }
                else
                {
                    assert( reading == LOW );
                    m_state = State_Rising;
                }
                m_prevState = State_Undefined;
                m_debounceStart = millis();
                break;
            case State_Low:
                if( reading == HIGH )
                {
                    m_state = State_Falling;
                    m_debounceStart = millis();
                }
                m_prevState = State_Low;
                break;
            case State_Falling:
                if( reading == LOW )
                {
                    m_state = m_prevState;
                }
                else
                {
                    if( millis() > m_debounceStart + debounceDelay)
                    {
                        m_state = State_High;
                        SerialLog::log("Transiting to HIGH state");
                    }
                }
                break;
            case State_High:
                if( reading == LOW )
                {
                    m_state = State_Rising;
                    m_debounceStart = millis();
                }
                m_prevState = State_High;
                break;
            case State_Rising:
                if( reading == HIGH )
                {
                    m_state = m_prevState;
                }
                else
                {
                    if( millis() > m_debounceStart + debounceDelay)
                    {
                        m_state = State_Low;
                        SerialLog::log("Transiting to LOW state");
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
        State_Undefined, // Used at startup when switch state is unknown
        State_Low,
        State_Falling,
        State_High,
        State_Rising,
    };

    uint8_t m_pin;
    State m_state = State_Undefined;
    State m_prevState = State_Undefined;
    unsigned long m_debounceStart;
    static const unsigned long debounceDelay = 50;    // in millis
};

// vim: sw=4:ts=4
