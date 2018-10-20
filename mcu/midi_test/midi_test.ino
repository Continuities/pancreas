#include <MIDIUSB.h>

#define MIDI_CHANNEL 0
#define MIDI_PITCH 48 // middle c
#define MIDI_VELOCITY 64

void setup() {
  // put your setup code here, to run once:
  
}

void loop() {
  // put your main code here, to run repeatedly:
  sendMidiBeat();
  delay(1000);
}

void sendMidiBeat() {
  midiEventPacket_t noteOn = {0x09, 0x90 | MIDI_CHANNEL, MIDI_PITCH, MIDI_VELOCITY};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}
