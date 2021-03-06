#include <DMXSerial.h>

#define DEBUG 1

int RedList[]   = {255, 128,   0,   0,   0, 128};
int GreenList[] = {  0, 128, 255, 128,   0,   0};
int BlueList[]  = {  0,   0,   0, 128, 255, 128};

int RedLevel, GreenLevel, BlueLevel;

int RedNow = 0;
int GreenNow = 0;
int BlueNow = 0;

int state = 0;

void setup() {
  DMXSerial.init(DMXController);
  DMXSerial.maxChannel(64);

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
}

void loop() {
  for (int v = 0; v <= 255; v++) {
    DMXSerial.write(1, v);
    DMXSerial.write(2, v);
    DMXSerial.write(3, v);
    DMXSerial.write(4, 255);
    DMXSerial.write(5, 255);
    DMXSerial.write(6, 255);
    DMXSerial.write(7, 255);
    DMXSerial.write(8, 255);
    //for (int c = 4; c <= 50; c++) {
    //  DMXSerial.write(c, v);
    //}
    delayMicroseconds(20000);
  }
}
