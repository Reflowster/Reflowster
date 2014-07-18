#include <Encoder.h>
#include <SPI.h>
#include <Adafruit_MAX31855.h>
#include <Adafruit_NeoPixel.h>
#include "Reflowster.h"

Reflowster reflowster;

void setup() {
  Serial.begin(9600);
  
  reflowster.init();

  reflowster.displayTest();
}

void loop() { 
  delay(1000);
}

