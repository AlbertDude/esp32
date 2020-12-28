// Modifications, additions by Albert Chau, 2020
// Original from: https://github.com/espressif/arduino-esp32/tree/master/libraries/Ticker
/*
  Ticker.h - esp32 library that calls functions periodically

  Copyright (c) 2017 Bert Melis. All rights reserved.

  Based on the original work of:
  Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  The original version is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef TICKER_H
#define TICKER_H

extern "C" {
  #include "esp_timer.h"
}

// Usage
//
//  Ticker ticker;  // Instantiate
//  ticker.attach   (period_s, fcn);  // calls void fcn() at specified seconds (float)
//  ticker.attach_ms(period_ms, fcn); // calls void fcn() at specified ms (uint32_t)
//  ticker.once   (period_s, fcn);  // calls void fcn() one time after specified seconds (float)
//  ticker.once_ms(period_ms, fcn); // calls void fcn() one time after specified ms (uint32_t)
//  ticker.detach();                // kill Ticker
//
class Ticker
{
public:
  Ticker()
  : _timer(nullptr)
  {};

  ~Ticker()
  { detach(); }

  typedef void (*callback_t)(void);
  typedef void (*callback_with_arg_t)(void*);

  void attach(float seconds, callback_t callback)
  {
    _attach_ms(seconds * 1000, true, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  void attach_ms(uint32_t milliseconds, callback_t callback)
  {
    _attach_ms(milliseconds, true, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  void attach_us(uint32_t microseconds, callback_t callback)
  {
    _attach_us(microseconds, true, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  template<typename TArg>
  void attach(float seconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach() callback argument size must be <= 4 bytes");
    // C-cast serves two purposes:
    // static_cast for smaller integer types,
    // reinterpret_cast + const_cast for pointer types
    uint32_t arg32 = (uint32_t)arg;
    _attach_ms(seconds * 1000, true, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  template<typename TArg>
  void attach_ms(uint32_t milliseconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach_ms() callback argument size must be <= 4 bytes");
    uint32_t arg32 = (uint32_t)arg;
    _attach_ms(milliseconds, true, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  template<typename TArg>
  void attach_us(uint32_t microseconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach_ms() callback argument size must be <= 4 bytes");
    uint32_t arg32 = (uint32_t)arg;
    _attach_us(microseconds, true, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  void once(float seconds, callback_t callback)
  {
    _attach_ms(seconds * 1000, false, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  void once_ms(uint32_t milliseconds, callback_t callback)
  {
    _attach_ms(milliseconds, false, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  void once_us(uint32_t microseconds, callback_t callback)
  {
    _attach_us(microseconds, false, reinterpret_cast<callback_with_arg_t>(callback), 0);
  }

  template<typename TArg>
  void once(float seconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach() callback argument size must be <= 4 bytes");
    uint32_t arg32 = (uint32_t)(arg);
    _attach_ms(seconds * 1000, false, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  template<typename TArg>
  void once_ms(uint32_t milliseconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach_ms() callback argument size must be <= 4 bytes");
    uint32_t arg32 = (uint32_t)(arg);
    _attach_ms(milliseconds, false, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  template<typename TArg>
  void once_us(uint32_t microseconds, void (*callback)(TArg), TArg arg)
  {
    static_assert(sizeof(TArg) <= sizeof(uint32_t), "attach_ms() callback argument size must be <= 4 bytes");
    uint32_t arg32 = (uint32_t)(arg);
    _attach_us(microseconds, false, reinterpret_cast<callback_with_arg_t>(callback), arg32);
  }

  void detach()
  {
    if (_timer) {
      esp_timer_stop(_timer);
      esp_timer_delete(_timer);
      _timer = nullptr;
    }
  };

  bool active()
  {
    return (_timer != nullptr);
  };

protected:
  void _attach_ms(uint32_t milliseconds, bool repeat, callback_with_arg_t callback, uint32_t arg)
  {
    _attach_us(milliseconds*1000, repeat, callback, arg);
  }

  void _attach_us(uint32_t microseconds, bool repeat, callback_with_arg_t callback, uint32_t arg)
  {
    esp_timer_create_args_t _timerConfig;
    _timerConfig.arg = reinterpret_cast<void*>(arg);
    _timerConfig.callback = callback;
    _timerConfig.dispatch_method = ESP_TIMER_TASK;
    _timerConfig.name = "Ticker";
    if (_timer) {
      esp_timer_stop(_timer);
      esp_timer_delete(_timer);
    }
    esp_timer_create(&_timerConfig, &_timer);
    if (repeat) {
      esp_timer_start_periodic(_timer, microseconds);
    } else {
      esp_timer_start_once(_timer, microseconds);
    }
  };



protected:
  esp_timer_handle_t _timer;
};


#endif  // TICKER_H
