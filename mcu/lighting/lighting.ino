#define FASTLED_INTERNAL
#include <FastLED.h>
#include <DmxSimple.h>
#include <MIDIUSB.h>

#define DEBUG_FADERS false
#define DEBUG_PULSE false
#define PULSE_MODE true
#define MIDI_PULSE true
#define PULSE_THRESHOLD 240
#define N_INPUTS 5
#define SAMPLES 10
#define PULSE_SAMPLE_RATE 100 // ms
#define PULSE_WINDOW 1000 // ms
#define PULSE_DEVIATION 500
#define BAUD 57600
#define LOGGING_INTERVAL 100 // ms
#define LIGHT_1 5 // dmx channel
#define LIGHT_2 9 // dmx channel
#define LIGHT_3 13 // dmx channel
#define MIDI_CHANNEL 0
#define MIDI_PITCH 48 // middle c
#define MIDI_VELOCITY 64
#define MAX_PULSE 128

static const uint8_t fader_pins[] = {A3, A2, A1, A0, A4};
static const uint8_t pulse_pin = A5;

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

enum states {UP, DOWN};

movingAvg avgs[N_INPUTS] = {0};
movingAvg pulse_avg;
unsigned long last_send_time = 0;
unsigned long last_pulse_sample = 0;
int state;
int16_t place_in_span;
uint16_t singleStrobeCounter;
uint16_t doubleStrobeCounter;
int prev_spd;
int pulse_low = 9999;
int pulse_high = 0;
int next_pulse_low = 9999;
int next_pulse_high = 0;
bool sent_midi = false;

void setup() {
  DmxSimple.usePin(3);  // double rainbow shield
  //DmxSimple.usePin(4);  // new, silver shield

  DmxSimple.maxChannel(8);

  Serial.begin(BAUD);
}

void loop() {
  int v[N_INPUTS];

  for (int i = 0; i < N_INPUTS; i++) {
    int raw = map(analogRead(fader_pins[i]), 1023, 0, 0, 1023);
    v[i] = compute_avg(&avgs[i], raw);
  }

  if (DEBUG_FADERS && millis() - last_send_time >= LOGGING_INTERVAL) {
    last_send_time += LOGGING_INTERVAL;

    Serial.print("cat ");
    for (int i = 0; i < N_INPUTS; i++) {
      Serial.print(v[i]);
      Serial.print(' ');
    }
    Serial.print("meow\n");
  }

  int pulse = normalize_pulse(analogRead(pulse_pin));

  if (MIDI_PULSE) {
    if (!sent_midi && pulse > PULSE_THRESHOLD) {
      sendMidiBeat();
      sent_midi = true;
    }
    else if (sent_midi && pulse < PULSE_THRESHOLD) {
      sent_midi = false;
    }
  }

  int mode = v[0];
  hsv colour;
  if (PULSE_MODE) {
//    simple_pulse(pulse);
//    dual_fade_pulse(pulse);
    triple_fade_pulse(pulse);
  } else if (mode < 256) {
    colour_fade(v);
  } else if (mode < 640) {
    single_strobe(v);
  } else {
    dual_strobe(v);
  }

  // wait for the ADC to settle (at least 2 ms):
  delay(2);
}

void sendMidiBeat() {
  midiEventPacket_t noteOn = {0x09, 0x90 | MIDI_CHANNEL, MIDI_PITCH, MIDI_VELOCITY};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void set_colour(hsv colour, int base_channel) {
  const CRGB& rgb = CHSV(colour.h, colour.s, colour.v);
  DmxSimple.write(base_channel, rgb.r);
  DmxSimple.write(base_channel + 1, rgb.g);
  DmxSimple.write(base_channel + 2, rgb.b);
  DmxSimple.write(base_channel + 3, 0);
}

int compute_avg(struct movingAvg *avg, int new_val) {
  avg->sum = avg->sum - avg->values[avg->pos] + new_val;
  avg->values[avg->pos] = new_val;
  avg->pos = (avg->pos + 1) % SAMPLES;
  if (avg->samples_in < SAMPLES) {
    avg->samples_in++;
  }
  return avg->sum / avg->samples_in;
}

int normalize_pulse(int raw) {
  if (millis() - last_pulse_sample >= PULSE_WINDOW) {
    last_pulse_sample = millis();
    pulse_low = next_pulse_low;
    pulse_high = next_pulse_high;
    next_pulse_high = 0;
    next_pulse_low = 9999;
  }

  if (raw < pulse_low) { pulse_low = raw; }
  if (raw < next_pulse_low) { next_pulse_low = raw; }
  if (raw > pulse_high) { pulse_high = raw; }
  if (raw > next_pulse_high) { next_pulse_high = raw; }

  int normalized = map(raw, pulse_low, pulse_high, 0, MAX_PULSE);

  if (DEBUG_PULSE) {
    Serial.print(normalized);
    Serial.print(' ');
    Serial.print(raw);
    Serial.print(' ');
    Serial.print(pulse_low);
    Serial.print(' ');
    Serial.println(pulse_high);
  }

  return normalized;
}

/** RENDER MODES **/
void simple_pulse(int normalized_pulse) {
  set_colour({
    .h = 0,
    .s = 255,
    .v = normalized_pulse
  }, LIGHT_1);
}

void dual_fade_pulse(int normalized_pulse) {
  set_colour({
    .h = 223,
    .s = 255,
    .v = MAX_PULSE - normalized_pulse  
  }, LIGHT_1);

  set_colour({
    .h = 0,
    .s = 255,
    .v = normalized_pulse
  }, LIGHT_2);
}

void triple_fade_pulse(int normalized_pulse) {

  int hue = map(normalized_pulse, 0, MAX_PULSE, 220, 400);
  if (hue > 255) {
    hue = 0; 
  }
  
  set_colour({
    .h = hue,
    .s = 255,
    .v = max(0, map(normalized_pulse, 0, MAX_PULSE / 2, MAX_PULSE, 0))
  }, LIGHT_1);

  set_colour({
    .h = hue,
    .s = 255,
    .v = normalized_pulse <= MAX_PULSE / 2 ? min(MAX_PULSE, map(normalized_pulse, 0, MAX_PULSE / 2, 0, MAX_PULSE)) : min(MAX_PULSE, map(normalized_pulse, MAX_PULSE / 2, MAX_PULSE, MAX_PULSE, 0))
  }, LIGHT_2);

  set_colour({
    .h = hue,
    .s = 255,
    .v = max(0, map(normalized_pulse, MAX_PULSE / 2, MAX_PULSE, 0, MAX_PULSE))
  }, LIGHT_3);
}

void colour_fade(int v[]) {
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

  set_colour({
    .h = hue1 + place_in_span / 256,
    .s = 255,
    //.s = map(v[4], 0, 1023, 0, 255),
    .v = 255
  }, LIGHT_1);
}

void single_strobe(int v[]) {
  int spd = map(v[2], 0, 1023, 1, 75);

  if (spd != prev_spd) {
    singleStrobeCounter = singleStrobeCounter % (2 * prev_spd);
    prev_spd = spd;
  }

  set_colour({
    .h = map(v[1], 0, 1023, 0, 255),
    .s = map(v[3], 0, 1023, 0, 255),
    .v = (++singleStrobeCounter % (2 * spd)) < spd ? 255 : 0
  }, LIGHT_1);
}

void dual_strobe(int v[]) {
  int spd = map(v[2], 0, 1023, 1, 75);
  int hue1 = map(v[1], 0, 1023, 0, 255);
  int hue2 = map(v[3], 0, 1023, 0, 255);

  if (spd != prev_spd) {
    doubleStrobeCounter = doubleStrobeCounter % (2 * prev_spd);
    prev_spd = spd;
  }

  set_colour({
    .h = (++doubleStrobeCounter % (2 * spd)) < spd ? hue1 : hue2,
    .s = 255,
    .v = 255
  }, LIGHT_1);
}
