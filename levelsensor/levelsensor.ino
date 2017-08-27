/*
    ULTRA SONIC LEVEL SENSOR FOR HOMEMATIC
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

// *** CONFIGURATION ***

// temperature of the air [°C]
const auto AIR_TEMPERATURE = 8.0;
// humidity of the air [%]
// (the effect is very low)
const auto AIR_HUMIDITY = 100.0;

// pins of the ultra sonic module
const auto TRIGGER_PIN = A0;
const auto ECHO_PIN = A1;

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
// (do not stress the HomeMatic transmitter to much, e.g. use 180 seconds)
const uint32_t MEASUREMENT_PAUSE = 120;

// start of range [m]
// (maximum supported: 5.5m)
const auto DISTANCE_RANGE_BEGIN = 2.33;
// end of range [m]
// (maximum supported: 5.5m, a value below DISTANCE_RANGE_BEGIN is allowed)
const auto DISTANCE_RANGE_END = 0.32;
// good quality distance [m]
// (max. distance from median, only good values are included in the final result)
const auto DISTANCE_GOOD_QUALITY = 0.015;
// the offset is added to the measured value [m]
// (the value depends on the used ultra sonic module, e.g. 0.017m for JSN-SR04T)
const auto DISTANCE_OFFSET = 0.017;

// number of additional echoes to skip
const auto NUM_SKIP_ECHOES = 1;
// number of samples
const auto NUM_SAMPLES = 10;
// mininum number of good samples;
const auto NUM_GOOD_SAMPLES = 5;

// enable debug messages over serial port
const auto DEBUG_ENABLE = true;
// baud rate for monitoring [bits/s]
const auto BAUD_RATE = 250000;

// *** CONSTANTS ***

// output start of range
const uint8_t OUT_RANGE_MIN = 0;
// output end of range
// (OUT_RANGE_MAX must be greater than OUT_RANGE_MIN)
const uint8_t OUT_RANGE_MAX = 254;
// invalid measurement value
const uint8_t OUT_INVALID_MEASUREMENT = 255;

// sonic speed [m/s]
const auto SONIC_SPEED = 331.3 + 0.6 * AIR_TEMPERATURE + 1.5 * AIR_HUMIDITY / 100.0;
// travelling time at start of range [µs/2]
// (the result of the expression must fit into uint16_t)
const uint16_t TIME_RANGE_BEGIN = DISTANCE_RANGE_BEGIN * 2.0 / SONIC_SPEED * 1000000.0 * 2.0;
// travelling time at end of range [µs/2]
// (the result of the expression must fit into uint16_t)
const uint16_t TIME_RANGE_END = DISTANCE_RANGE_END * 2.0 / SONIC_SPEED * 1000000.0 * 2.0;
// good quality time difference [µs/2]
const uint16_t TIME_GOOD_QUALITY = DISTANCE_GOOD_QUALITY * 2.0 / SONIC_SPEED * 1000000.0 * 2.0;
// time offset [µs/2]
// (will be added to the measured time)
const uint16_t TIME_OFFSET = DISTANCE_OFFSET * 2.0 / SONIC_SPEED * 1000000.0 * 2.0;
// maximum expected time [µs/2]
const uint16_t TIME_MIN = min(TIME_RANGE_BEGIN, TIME_RANGE_END);
// minimum expected time [µs/2]
const uint16_t TIME_MAX = max(TIME_RANGE_BEGIN, TIME_RANGE_END);
// time precision of the out range [µs/2]
const int16_t TIME_PRECISION = (int32_t(TIME_RANGE_END) - TIME_RANGE_BEGIN) / (OUT_RANGE_MAX - OUT_RANGE_MIN);
// pause between pings [ms]
const uint32_t PING_PAUSE = (uint32_t(NUM_SKIP_ECHOES) * TIME_MAX / 2 + 999) / 1000;
// invalid measurement marker
const uint16_t INVALID_MEASUREMENT = 0;

// *** GLOBAL VARIABLES ***

// measured samples
uint16_t samples[NUM_SAMPLES];
// points behind the last valid sample
// (if samplesEnd == samples, the array is empty)
uint16_t *samplesEnd;

// *** FUNCIONS ***

// make one ping
// returns:
//   INVALID:   measurement error
//   otherwise: travelling time [µs/2]
uint16_t ping() {
  if (digitalRead(ECHO_PIN)) return INVALID_MEASUREMENT;
  // send trigger pulse
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  if (digitalRead(ECHO_PIN)) return INVALID_MEASUREMENT;
  // higher precision, if interrupts are switched off
  // (this is not allowed on devices with a watchdog active, e.g. Wemos D1)
  noInterrupts();
  // measure pulse length
  auto t = pulseIn(ECHO_PIN, HIGH, TIME_MAX) * 2;
  interrupts();
  if (t == 0) return INVALID_MEASUREMENT;
  t += TIME_OFFSET;
  return t;
}

// prints a comma separated list of the samples
void printSamples() {
  for (auto it = samples; it < samplesEnd; ) {
    Serial.print(*it++);
    if (it < samplesEnd) Serial.print(F(","));
  }
  Serial.println();
}

// fills the samples array
void rawMeasure() {
  samplesEnd = samples;
  for (int cnt = 0; cnt < NUM_SAMPLES; ++cnt) {
    // pause between pings to skip echoes
    if (cnt > 0) delay(PING_PAUSE);
    auto t = ping();
    // store only valid samples
    if (t != INVALID_MEASUREMENT) *samplesEnd++ = t;
  }
}

// remove samples out of range
void removeOutOfRange() {
  // filter array entries
  auto dstIt = samples;
  for (auto srcIt = samples; srcIt < samplesEnd; ++srcIt)
    if (*srcIt >= TIME_MIN && *srcIt <= TIME_MAX) *dstIt++=*srcIt;
  samplesEnd = dstIt;
}

// needed for sorting via qsort()
int compare_uint16(const void *a_ptr, const void *b_ptr) {
  auto a = *(uint16_t *)a_ptr, b = *(uint16_t *)b_ptr;
  return a > b ? 1 : (a < b ? -1 : 0);
}

// calculates the median
uint16_t median() {
  auto size = samplesEnd - samples;
  if (size) {
    // sort
    qsort(samples, size, sizeof(uint16_t), compare_uint16);
    auto mid = samples + size / 2;
    // for odd array lengths return the center element 
    if (size & 1) return *mid;
    // for even array lengths return the average of the two center elements
    else return (uint32_t(*mid) + uint32_t(*(mid-1))) / 2;
  } else return 0;
}

// remove spikes
void removeSpikes(uint16_t median) {
  auto lowerLimit = median - TIME_GOOD_QUALITY;
  auto upperLimit = median + TIME_GOOD_QUALITY;
  // filter array entries
  auto dstIt = samples;
  for (auto srcIt = samples; srcIt < samplesEnd; ++srcIt)
    if (*srcIt >= lowerLimit && *srcIt <= upperLimit) *dstIt++=*srcIt;
  samplesEnd = dstIt;
}

// calculates the average
uint16_t average() {
  auto size = samplesEnd - samples;
  if (size) {
    uint32_t sum = 0;
    for (auto it = samples; it < samplesEnd; it++) sum += *it;
    return sum / uint32_t(size);
  } else return 0;
}

// maps the measured time to the output range
// in: measured time [µs/2]
uint8_t mapToOutRange(uint16_t in) {
  // map with rounding to nearest
  in += TIME_PRECISION / 2;
  return map(in, TIME_RANGE_BEGIN, TIME_RANGE_END, OUT_RANGE_MIN, OUT_RANGE_MAX);
}

// maps the measured time to distance
// in: measured time [µs/2]
// returns: distance [mm]
uint16_t mapToDistance(uint16_t in) {
  return map(in, TIME_RANGE_BEGIN, TIME_RANGE_END, DISTANCE_RANGE_BEGIN * 1000, DISTANCE_RANGE_END * 1000);
}

uint8_t measure() {
  // fill the samples array
  rawMeasure();
  if (DEBUG_ENABLE) {
    Serial.print(F("MEASURED[µs/2]: ")); printSamples();
  }

  // remove out of range samples
  removeOutOfRange();
  if (DEBUG_ENABLE) {
    Serial.print(F("IN RANGE[µs/2]: ")); printSamples();
  }
  
  // median
  auto med = median();
  if (DEBUG_ENABLE) {
    Serial.print(F("SORTED  [µs/2]: ")); printSamples();
    Serial.print(F("MEDIAN  [µs/2]: ")); Serial.println(med);
  }

  // remove spikes
  removeSpikes(med);
  if (DEBUG_ENABLE) {
    Serial.print(F("CLEANED [µs/2]: ")); printSamples();
  }

  // average
  auto avg = average();
  if (DEBUG_ENABLE) {
    Serial.print(F("AVERAGE [µs/2]: ")); Serial.println(avg);
  }

  // map to output range
  uint16_t distance; // [mm]
  uint8_t out;
  // minimum required samples?
  if (samplesEnd - samples >= NUM_GOOD_SAMPLES) {
    distance = mapToDistance(avg);
    out = mapToOutRange(avg);
  } else {
    distance = 0;
    out = OUT_INVALID_MEASUREMENT;
  }
  if (DEBUG_ENABLE) {
    Serial.print(F("DISTANCE  [mm]: ")); Serial.println(distance);
    Serial.print(F("OUT           : ")); Serial.println(out);
  }
  return out;
}

void send(uint8_t meas) {
  PORTB = (PORTB & 0b11100000) | (meas & 0b00011111);
  PORTD = (PORTD & 0b00011111) | (meas & 0b11100000);
}

void blinkLed() {
  for (int idx = 0; idx < 3; ++idx) {
    digitalWrite(LED_BUILTIN, LOW); delay(100);
    digitalWrite(LED_BUILTIN, HIGH); delay(100);
  }
  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // ultra sonic module
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  // HomeMatic 8-bit transmitter
  // set PB0 - PB4 and PD5 - PD7 to output
  DDRB |= 0b00011111;
  DDRD |= 0b11100000;
  send(OUT_INVALID_MEASUREMENT);
  
  if (DEBUG_ENABLE) {
    Serial.begin(BAUD_RATE);
    Serial.println(F("*** ULTRA SONIC LEVEL SENSOR ***"));
    Serial.print(F("DISTANCE_RANGE_BEGIN[mm]: ")); Serial.println(uint16_t(DISTANCE_RANGE_BEGIN * 1000));
    Serial.print(F("DISTANCE_RANGE_END  [mm]: ")); Serial.println(uint16_t(DISTANCE_RANGE_END * 1000));
    Serial.print(F("TIME_RANGE_BEGIN  [µs/2]: ")); Serial.println(TIME_RANGE_BEGIN);
    Serial.print(F("TIME_RANGE_END    [µs/2]: ")); Serial.println(TIME_RANGE_END);
  }
}

void loop() {
  if (DEBUG_ENABLE) Serial.println();
  
  // turn led on
  digitalWrite(LED_BUILTIN, HIGH);

  // measure and send
  uint8_t meas = measure();
  send(meas);

  // invalid measurement?
  if (meas == OUT_INVALID_MEASUREMENT) blinkLed(); 
  else digitalWrite(LED_BUILTIN, LOW);

  // pause for next measurement
  delay(MEASUREMENT_PAUSE * 1000l);
}

