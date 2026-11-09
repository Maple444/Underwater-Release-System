# Underwater-Release-System
A time based underwater release mechanism. This repo stores the scripts and libraries for coding the on the ESP32-c6, this code should not have any glitches exept* for not pressing the rst fully,
this definately maybe fixed soon. the esp32 does seem to work with ble and pwm interrupts better.

Update: 20/10/2026 the arduino code built off the preexisting Release code, note the Release_Motor_Control_FINAL.ino file is 
the old code using the arduino nano BLE, new code/ versions will be uploaded when complete.

(add BLE and NTP sync libraries)

update: 
the esp32 does finish a battery faster than the arduino, but it should last <4 days with a 2800mah 18650, I may use a esp board with more sturdy battery inputs.
