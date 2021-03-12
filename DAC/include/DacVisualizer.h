
// DAC Audio Data Visualizer
// - visualizes the audio data playing thru a DAC instance
// - queries the DAC instance to get current play position, calculates a level from a window of
// samples around the play position
// - currently configured to output 6 levels [0, 5], intended to drive a 10-element LED bar display
// symmetrically
// - drives outputs HIGH to light up output LEDs
//  - (ESP32 has higher current source capabilities than sink)
//  - see: https://www.esp32.com/viewtopic.php?t=5840#p71756

/*
              ╔═════════════════════════════════════╗      
              ║            ESP-WROOM-32             ║      
              ║               Devkit                ║      
              ║                                     ║      
              ║EN /                         MOSI/D23║  
              ║VP /A0                        SCL/D22║──I2SOut
              ║VN /A3                         TX/TX0║  
              ║D34/A6                         RX/RX0║                       ↗↗      
              ║D35/A7                        SDA/D21║──GPIO21   level=5 ────▶────R───┐           
              ║D32/A4,T9                    MISO/D19║──GPIO19   level=4     "    "   ▽ Gnd
              ║D33/A5,T8                     SCK/D18║──GPIO18   level=3     "    "    
        DAC1──║D25/A18,DAC1                   SS/ D5║                             
              ║D26/A19,DAC2                     /TX2║──GPIO17   level=2     "    "  
              ║D27/A17,T7                       /RX2║──GPIO16   level=1     "    "  
              ║D14/A16,T6                 T0,A10/ D4║      
              ║D12/A15,T5     LED_BUILTIN,T2,A12/ D2║
              ║D13/A14,T4                 T3,A13/D15║
              ║GND/                             /GND║
         VIN──║VIN/                             /3V3║
              ║                                     ║      
              ║   EN           μUSB           BOOT  ║      
              ╚═════════════════════════════════════╝      
*/

#pragma once

#include "../../SerialLog/include/SerialLog.h"
#include "../../Ticker/include/Ticker.h"
#include "Dac.h"
#include <assert.h>
#include <Arduino.h>

// Helpers

//
class DacVisualizer
{
public:
#   define WINDOW_DURATION_S (0.05)
#   define WINDOW_OVERLAP_FRACTION (0.5)

    DacVisualizer()
    {
        for(int i=1; i<m_num_levels; i++)
        {
            pinMode(m_output_pins[i], OUTPUT);
        }
        visualize(0);
    }

    DacVisualizer(IDac *dacInstance)
        : DacVisualizer()
    {
        assert( dacInstance != nullptr );
        m_DacInstance = dacInstance;
    }

    void reset(IDac *dacInstance)
    {
        assert( dacInstance != nullptr );
        m_DacInstance = dacInstance;
        m_active = false;

        m_window_duration = (unsigned int) (WINDOW_DURATION_S * m_DacInstance->getSamplerate());
        m_window_overlap = (unsigned int) (WINDOW_OVERLAP_FRACTION * m_window_duration);
        m_window_interval = m_window_duration - m_window_overlap;

        m_buffer_len = m_DacInstance->getDataBufferLen();

        unsigned int max_amp;
        if( m_DacInstance->getBitsPerSample() == 8 )
        {
            m_dc_ofs = 128;
            max_amp = 128;
        }
        else
        {
            assert( m_DacInstance->getBitsPerSample() == 16 );
            m_dc_ofs = 0;
            max_amp = 32768;
        }
        assert( m_num_levels > 0 );
        m_iscale = (max_amp / m_num_levels) + (max_amp % m_num_levels != 0);    // ceil (max_amp/#levels)

        m_interval_index = 0;
        _update_interval_range();
    }

    ~DacVisualizer() {}

    void _update_interval_range()
    {
        m_interval_end   = (m_interval_index+1) * m_window_interval;
        m_interval_start =  (int)m_interval_end - (int)m_window_duration;
        //SerialLog::log( "Interval start,end:" + String(m_interval_start) + ", " + String(m_interval_end) );

        // "start" progress-point
        m_interval_progress_point = m_interval_start;

        // "mid" progress-point
        //m_interval_progress_point = (m_interval_start + m_interval_end) / 2;

        // "end" progress-point
        //m_interval_progress_point = m_interval_end;

    }

    void increment_interval()
    {
        m_interval_index++;
        _update_interval_range();
    }

    void _find_extrema(unsigned int start_idx, unsigned int end_idx, int & max_val, int & min_val)
    {
        assert( start_idx >= 0 );
        assert( start_idx < end_idx );
        assert( end_idx <= m_buffer_len );

        int min_so_far = 100000;
        int max_so_far = -100000;

        if( m_DacInstance->getBitsPerSample() == 8 )
        {
            const uint8_t * p8 = reinterpret_cast<const uint8_t*>(m_DacInstance->getDataBuffer());
            for( auto i=start_idx; i<end_idx; i++ )
            {
                if( p8[i] > max_so_far )
                    max_so_far = p8[i];
                if( p8[i] < min_so_far )
                    min_so_far = p8[i];
            }
        }
        else
        {
            assert( m_DacInstance->getBitsPerSample() == 16 );
            const int16_t * p16 = reinterpret_cast<const int16_t*>(m_DacInstance->getDataBuffer()); 
            for( auto i=start_idx; i<end_idx; i++ )
            {
                if( p16[i] > max_so_far )
                    max_so_far = p16[i];
                if( p16[i] < min_so_far )
                    min_so_far = p16[i];
            }
        }
        max_val = max_so_far;
        min_val = min_so_far;
    }

    unsigned int calc_value()
    {
        unsigned int ret_val = 0;

        unsigned int start_idx = max( 0, m_interval_start );
        unsigned int end_idx   = min( m_buffer_len, m_interval_end );

        if( end_idx <= start_idx )
        {
            // this is possible with progress_point of "start"
            // e.g. for "gameOverMan.wav"
            return ret_val;
        }
        //SerialLog::log( "Interval idxs:" + String(start_idx) + ", " + String(end_idx) );

        int max_val;
        int min_val;
        _find_extrema(start_idx, end_idx, max_val, min_val);
        max_val -= m_dc_ofs;
        min_val -= m_dc_ofs;
        int max_amp = max(max_val, abs(min_val));
        assert( max_amp >= 0 );
        ret_val = max_amp / m_iscale;
        assert( ret_val >= 0 );
        assert( ret_val < m_num_levels );

        return ret_val;
    }

    void visualize(unsigned int value)
    {
        assert( value < m_num_levels );
        int i=1;
        for(; i<=value; i++)
        {
            digitalWrite(m_output_pins[i], HIGH);
        }
        for(; i<m_num_levels; i++)
        {
            digitalWrite(m_output_pins[i], LOW);
        }
    }

    void debug_visualize(unsigned int value)
    {
        assert( value < m_num_levels );

        //SerialLog::log( "Value: " + String(value) );
#if 1
        // Debug diagnostics
        // - 2-ended bar-graph style
        String out_str = "";
        for( int i=0; i<m_num_levels - value; i++ )
            out_str += " ";
        for( int i=0; i<value; i++ )
            out_str += "<";
        for( int i=0; i<value; i++ )
            out_str += ">";
        for( int i=0; i<m_num_levels - value; i++ )
            out_str += " ";
        SerialLog::log( out_str );
#endif
    }

    void loop()
    {
        assert( m_DacInstance );
        unsigned int cur_sample_pos = m_DacInstance->getCurrentPos();
        if( cur_sample_pos < m_buffer_len ) // TODO: not sure if this is correct logic
        {
            if( (int)cur_sample_pos >= m_interval_progress_point )
            {
                unsigned int value = calc_value();
                visualize(value);
                //debug_visualize(value);
                increment_interval();
                m_active = true;
            }
        }
        else
        {
            if( m_active )
            {
                visualize(0);
                m_active = false;
            }
        }
    }

private:
    IDac *m_DacInstance = nullptr;
    static const unsigned int m_num_levels = 6;
    uint8_t m_output_pins[m_num_levels] = {0xff, 16, 17, 18, 19, 21};
    unsigned int m_window_duration;
    unsigned int m_window_overlap;
    unsigned int m_window_interval;

    unsigned int m_buffer_len;
    unsigned int m_dc_ofs = 0;
    unsigned int m_iscale;

    unsigned int m_interval_index = 0;
    unsigned int m_interval_end;
    int m_interval_start;
    int m_interval_progress_point;
    bool m_active = false;
};

// vim: sw=4:ts=4
