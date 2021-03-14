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
#       - also provides a DacVisualizer class to visualize the audio data that is being played
#   Player
#       - press switch to advance to next play item
#       - uses DacT audio output
#       - cycles thru some 8kHz 8-bit audio tracks
#   mySAM
#       - usage of SAM TTS, speaking several canned phrases with the available voices
#       - press switch to advance to thru phrases
#           - when all phrases spoken, changes to next voice
#       - generate PCM to memory and then plays memory out to DAC
#       - quality not great but GEFN (Good Enough For Now)
#           - SQ does have some variance among the voices
### 
# On deck:
#   web server? - web i/f to enter speech phrases
#   terminal/ssh server? - basically way to get remote keyboard input
#   MQTT? - basically way to get remote keyboard input

#   myTTS
#       - hoping provides better sound quality than SAM
#   TTS
#   myAudio
#       - my version of PlayWAVFromPROGMEM.cpp from the ESP8266Audio package
#       - plays out the viola sample

