#ifndef Reflowster_h
#define Reflowster_h

#include "Arduino.h"
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Encoder.h>
#include "ReflowDisplay.h"

class Adafruit_MAX31855;

class Reflowster {
public:
  int pinConfiguration_statusLed;
  int pinConfiguration_relay;
  int pinConfiguration_encoderButton;
  int pinConfiguration_encoderA;
  int pinConfiguration_encoderB;
  int pinConfiguration_backButton;
  int pinConfiguration_displayDS;
  int pinConfiguration_displaySTCP;
  int pinConfiguration_displaySHCP;
  int pinConfiguration_displayD1;
  int pinConfiguration_displayD2;
  int pinConfiguration_displayD3;
  int pinConfiguration_displayDL;
  int pinConfiguration_beep;
  int pinConfiguration_thermocoupleCS;
  int pinConfiguration_thermocoupleSCK;
  int pinConfiguration_thermocoupleMISO;
  
  Reflowster();
  void init();
  void selfTest();
  void tick();
  void pulseTick();
  void beep(int freq, int duration);
  void setStatusColor(byte r, byte g, byte b);
  void setStatusPulse(byte r, byte g, byte b);
  ReflowDisplay * getDisplay();
  void displayTest();
  boolean getBackButton();
  boolean getButton();
  void setKnobPosition(int val);
  int getKnobPosition();
  double readCelsius();
  double readFahrenheit();
  void relayOn();
  void relayOff();
  void relayToggle();
  boolean relayStatus();
  
private:
  Adafruit_NeoPixel * status;
  Encoder * knob;
  Adafruit_MAX31855 * probe;
  ReflowDisplay * display;
  
  int pulseStatus;
  byte pulseColor[3];
  
  void handlePulse();
};

#endif

