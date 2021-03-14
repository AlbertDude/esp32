
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
        _Visualize(0);
    }

    DacVisualizer(IDac *dac_instance)
        : DacVisualizer()
    {
        assert( dac_instance != nullptr );
        dac_instance_ = dac_instance;
    }

    void Reset(IDac *dac_instance)
    {
        assert( dac_instance != nullptr );
        dac_instance_ = dac_instance;
        is_active_ = false;

        window_duration_ = (unsigned int) (WINDOW_DURATION_S * dac_instance_->GetSamplerate());
        window_overlap_  = (unsigned int) (WINDOW_OVERLAP_FRACTION * window_duration_);
        window_interval_ = window_duration_ - window_overlap_;

        m_buffer_len = dac_instance_->GetDataBufferLen();

        unsigned int max_amp;
        if( dac_instance_->GetBitsPerSample() == 8 )
        {
            m_dc_ofs = 128;
            max_amp = 128;
        }
        else
        {
            assert( dac_instance_->GetBitsPerSample() == 16 );
            m_dc_ofs = 0;
            max_amp = 32768;
        }
        assert( m_num_levels > 0 );
        m_iscale = (max_amp / m_num_levels) + (max_amp % m_num_levels != 0);    // ceil (max_amp/#levels)

        m_interval_index = 0;
        _UpdateIntervalRange();
    }

    ~DacVisualizer() {}

    void Loop()
    {
        if( dac_instance_ )
        {
            unsigned int cur_sample_pos = dac_instance_->GetCurrentPos();
            if( cur_sample_pos < m_buffer_len )
            {
                if( (int)cur_sample_pos >= m_interval_progress_point )
                {
                    unsigned int value = _CalcValue();
                    _Visualize(value);
                    //_DebugVisualize(value);
                    _IncrementInterval();
                    is_active_ = true;
                }
            }
            else
            {
                if( is_active_ )
                {
                    _Visualize(0);
                    is_active_ = false;
                }
            }
        }
    }

private:
    void _UpdateIntervalRange()
    {
        m_interval_end   = (m_interval_index+1) * window_interval_;
        m_interval_start =  (int)m_interval_end - (int)window_duration_;
        //SerialLog::Log( "Interval start,end:" + String(m_interval_start) + ", " + String(m_interval_end) );

        // "start" progress-point
        m_interval_progress_point = m_interval_start;

        // "mid" progress-point
        //m_interval_progress_point = (m_interval_start + m_interval_end) / 2;

        // "end" progress-point
        //m_interval_progress_point = m_interval_end;

    }

    void _IncrementInterval()
    {
        m_interval_index++;
        _UpdateIntervalRange();
    }

    void _FindExtrema(unsigned int start_idx, unsigned int end_idx, int & max_val, int & min_val)
    {
        assert( start_idx >= 0 );
        assert( start_idx < end_idx );
        assert( end_idx <= m_buffer_len );

        int min_so_far = 100000;
        int max_so_far = -100000;

        if( dac_instance_->GetBitsPerSample() == 8 )
        {
            const uint8_t * p8 = reinterpret_cast<const uint8_t*>(dac_instance_->GetDataBuffer());
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
            assert( dac_instance_->GetBitsPerSample() == 16 );
            const int16_t * p16 = reinterpret_cast<const int16_t*>(dac_instance_->GetDataBuffer()); 
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

    unsigned int _CalcValue()
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
        //SerialLog::Log( "Interval idxs:" + String(start_idx) + ", " + String(end_idx) );

        int max_val;
        int min_val;
        _FindExtrema(start_idx, end_idx, max_val, min_val);
        max_val -= m_dc_ofs;
        min_val -= m_dc_ofs;
        int max_amp = max(max_val, abs(min_val));
        assert( max_amp >= 0 );
        ret_val = max_amp / m_iscale;
        assert( ret_val >= 0 );
        assert( ret_val < m_num_levels );

        return ret_val;
    }

    void _Visualize(unsigned int value)
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

    void _DebugVisualize(unsigned int value)
    {
        assert( value < m_num_levels );

        //SerialLog::Log( "Value: " + String(value) );
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
        SerialLog::Log( out_str );
#endif
    }


private:
    IDac *dac_instance_ = nullptr;
    static const unsigned int m_num_levels = 6;
    uint8_t m_output_pins[m_num_levels] = {0xff, 16, 17, 18, 19, 21};

    unsigned int window_duration_;  // in samples
    unsigned int window_overlap_;   // in samples
    unsigned int window_interval_;  // in samples

    unsigned int m_buffer_len;

    unsigned int m_dc_ofs = 0;
    unsigned int m_iscale;

    unsigned int m_interval_index = 0;
    unsigned int m_interval_end;
    int m_interval_start;
    int m_interval_progress_point;

    bool is_active_ = false;
};

// vim: sw=4:ts=4
