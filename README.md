# WiFiClock
Simple WiFi (NTP) clock based on ESP8266 and LED matrix 32*8 on MAX7219

Connect LED matrix to SPI interface (D5, D7, D8 on Wemos D1 mini).
Connect SHT30/31 to I2C interface (D1 and D2).

To launch captive portal, reboot board 3 times.
To reset configuration, reboot board 5 times.

Default captive portal password is "1029384756".
Default administrator name is "admin", password is "12345678" (may be changed on "WiFi" page).
