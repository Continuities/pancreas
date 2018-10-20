#define FASTLED_INTERNAL
#include <FastLED.h>
#include <DmxSimple.h>

#define DEBUG_FADERS false
#define DEBUG_PULSE true
#define PULSE_MODE true
#define N_INPUTS 5
static const uint8_t fader_pins[] = {A3,A2,A1,A0,A4};
static const uint8_t pulse_pin = A5;
#define SAMPLES 10
#define BAUD 57600
#define SEND_INTERVAL 100 // ms

typedef struct hsv {
  uint8_t h;
  uint8_t s;
  uint8_t v;
} hsv;

typedef struct movingAvg {
  int values[SAMPLES];
  long sum;
  int pos;
  int samples_in;
} movingAvg;

movingAvg avgs[N_INPUTS] = {0};

int compute_avg(struct movingAvg *avg, int new_val) {
  avg->sum = avg->sum - avg->values[avg->pos] + new_val;
  avg->values[avg->pos] = new_val;
  avg->pos = (avg->pos + 1) % SAMPLES;
  if (avg->samples_in < SAMPLES) {
    avg->samples_in++;
  }
  return avg->sum / avg->samples_in;
}

void setup() {
  DmxSimple.usePin(3);  // double rainbow shield
  //DmxSimple.usePin(4);  // new, silver shield

  DmxSimple.maxChannel(8);

  Serial.begin(BAUD);
}

unsigned long last_send_time = 0;

enum states {UP, DOWN};
int state;
int16_t place_in_span;
uint16_t singleStrobeCounter;
uint16_t doubleStrobeCounter;
int prev_spd;
uint16_t low_pulse = 9999999;
uint16_t high_pulse = 0;

/** COLOUR MODES **/
hsv handle_pulse() {
  uint16_t data = analogRead(pulse_pin);
  if (DEBUG_PULSE) {
    Serial.println(data);
  }

  if (data < low_pulse) {
    low_pulse = data;
  }

  if (data > high_pulse) {
    high_pulse = data;
  }

  return {
    .h = 0,
    .s = 255,
    .v = map(data, low_pulse, high_pulse, 0, 255)
  };
  
}

hsv colour_fade(int v[]) {
  int hue1 = map(v[1], 0, 1023, 0, 255);
  int spd = map(v[2], 0, 1023, 32, 2048);
  int hue2 = map(v[3], 0, 1023, 0, 255);

  if (hue1 > hue2) {
    int swap = hue1;
    hue1 = hue2;
    hue2 = swap;
  }

  int16_t scaled_span = (hue2 - hue1) * 128;

  if (state == UP) {
    place_in_span += scaled_span / spd;
  } else if (state == DOWN) {
    place_in_span -= scaled_span / spd;
  } else {
    // shouldn't happen, so let's fix it
    state = UP;
  }

  if (place_in_span > scaled_span) {
    state = DOWN;
  } else if (place_in_span < 0) {
    state = UP;
  }

  return {
    .h = hue1 + place_in_span / 256,
    .s = 255,
    //.s = map(v[4], 0, 1023, 0, 255),
    .v = 255
  };
}

hsv single_strobe(int v[]) {
  int spd = map(v[2], 0, 1023, 1, 75);
  
  if (spd != prev_spd) {
    singleStrobeCounter = singleStrobeCounter % (2*prev_spd);
    prev_spd = spd;
  }

  return {
    .h = map(v[1], 0, 1023, 0, 255),
    .s = map(v[3], 0, 1023, 0, 255),
    .v = (++singleStrobeCounter % (2*spd)) < spd ? 255 : 0
  };
}

hsv dual_strobe(int v[]) {
  int spd = map(v[2], 0, 1023, 1, 75);
  int hue1 = map(v[1], 0, 1023, 0, 255);
  int hue2 = map(v[3], 0, 1023, 0, 255);

  if (spd != prev_spd) {
    doubleStrobeCounter = doubleStrobeCounter % (2*prev_spd);
    prev_spd = spd;
  }

  return {
    .h = (++doubleStrobeCounter % (2*spd)) < spd ? hue1 : hue2,
    .s = 255,
    .v = 255
  };
}

/** END COLOUR MODES **/

void do_the_colour_thing(hsv colour) {
  const CRGB& rgb = CHSV(colour.h, colour.s, colour.v);

  // ColorKey
  DmxSimple.write(1, rgb.r);
  DmxSimple.write(2, rgb.g);
  DmxSimple.write(3, rgb.b);
  DmxSimple.write(4, 255);

  // ADJ
  DmxSimple.write(5, rgb.r);
  DmxSimple.write(6, rgb.g);
  DmxSimple.write(7, rgb.b);
  DmxSimple.write(8, 0);
}

void loop() {
  int v[N_INPUTS];

  for (int i = 0; i < N_INPUTS; i++) {
     int raw = map(analogRead(fader_pins[i]), 1023, 0, 0, 1023);
     v[i] = compute_avg(&avgs[i], raw);
  }

  if (DEBUG_FADERS && millis() - last_send_time >= SEND_INTERVAL) {
    last_send_time += SEND_INTERVAL;

    Serial.print("cat ");
    for (int i = 0; i < N_INPUTS; i++) {
      Serial.print(v[i]);
      Serial.print(' ');
    }
    Serial.print("meow\n");
  }

  int mode = v[0];
  hsv colour;
  if (PULSE_MODE) {
    colour = handle_pulse();
  } else if (mode < 256) {
    colour = colour_fade(v);
  } else if (mode < 640) {
    colour = single_strobe(v);
  } else {
    colour = dual_strobe(v);
  }

  do_the_colour_thing(colour);

  // wait for the ADC to settle (at least 2 ms):
  delay(2);
}
