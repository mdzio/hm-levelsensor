/*
    HOMEMATIC 8-BIT TRANSMITTER TEST
    Copyright (C) 2017 MDZ (info@ccu-historian.de)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <avr/pgmspace.h>

// *** CONFIGURATION ***

// HomeMatic 8-bit transmitter HM-MOD-EM-8Bit
// (two ports are needed, because pins PD0 (RX) and PD1 (TX) are already used for debug messages)
// HM-MOD-EM-8Bit | Arduino Nano V3
// --------------------------------
// D0             |  8 (PB0)
// D1             |  9 (PB1)
// D2             | 10 (PB2)
// D3             | 11 (PB3)
// D4             | 12 (PB4)
// D5             |  5 (PD5)
// D6             |  6 (PD6)
// D7             |  7 (PD7)

// pause between measurements [s]
// (do not stress the HomeMatic transmitter to much, e.g. use 20 seconds)
const uint32_t SEND_PAUSE = 10;
// enable debug messages over serial port
const auto DEBUG_ENABLE = true;
// baud rate for monitoring [bits/s]
const auto BAUD_RATE = 250000;

// *** GLOBAL VARIABLES ***

uint8_t patternIdx;
const uint8_t patterns[] PROGMEM = {
  0b00000000,
  0b00000001,
  0b00000010,
  0b00000100,
  0b00001000,
  0b00010000,
  0b00100000,
  0b01000000,
  0b10000000,
  0b11111111  
};

// *** FUNCIONS ***

// sends value to the HomeMatic transmitter
void send(uint8_t meas) {
  PORTB = (PORTB & 0b11100000) | (meas & 0b00011111);
  PORTD = (PORTD & 0b00011111) | (meas & 0b11100000);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // HomeMatic 8-bit transmitter
  // set PB0 - PB4 and PD5 - PD7 to output
  DDRB |= 0b00011111;
  DDRD |= 0b11100000;
  
  if (DEBUG_ENABLE) {
    Serial.begin(BAUD_RATE);
    Serial.println(F("*** HOMEMATIC 8-BIT TRANSMITTER TEST ***"));
  }
}

void loop() {
  // send bit patterns 
  uint8_t pattern = pgm_read_byte(patterns + patternIdx++);
  if (patternIdx == sizeof(patterns)/sizeof(uint8_t) ) patternIdx = 0;
  send(pattern);
  
  if (DEBUG_ENABLE) {
    Serial.print(pattern);
    Serial.print(F("\t"));
    Serial.print(pattern, HEX);
    Serial.print(F("h\t"));
    Serial.print(pattern, BIN);
    Serial.println(F("b"));
  }

  // blink led
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  
  // pause for next measurement
  delay(SEND_PAUSE * 1000l - 250);
}

