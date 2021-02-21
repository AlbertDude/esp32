# esp32
# Various ESP32-Arduino samples and projects
#
# 
#   SerialLog
#       - Logging helper
#       - singleton wrapper around Serial.prints with timestamps
#   LoopTimer
#       - Performance profiling for loop()
#       - Reports on number of calls/sec over the specified reporting interval
#   Ticker
#       - my minor mods of Ticker.h - esp32 library that calls functions periodically
#       - Original from: https://github.com/espressif/arduino-esp32/tree/master/libraries/Ticker
#   Blinker
#       - toggles specified GPIO pin at specified interval, useful for blinking LEDs
#   Switch
#       - Switch reading helper class
#       - Provides debounced high/low readings for interfacing with electrical switches
#   DAC
#       - press switch to advance to next play item
#       - DAC audio output
#       - A few output options
#           - 8-bit DAC output polled-implementation
#           - 8-bit DAC output ticker-implementation
#               - drives speaker audio amp circuit (e.g. LM386)
#           - 1-bit sigma-delta output 
#               - drives speaker via 1R-1Q circuit
#   Player
#       - press switch to advance to next play item
#       - uses DacT audio output
#       - cycles thru some 8kHz 8-bit audio tracks
#


#   TTS
#   myAudio
#   mySAM
#   myTTS

