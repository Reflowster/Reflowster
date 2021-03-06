#include <Arduino.h>
#include <SPI.h>
#include <Encoder.h>
#include <Adafruit_MAX31855.h>
#include <Adafruit_NeoPixel.h>
#include "ReflowDisplay.h"

#include "Reflowster.h"

Reflowster::Reflowster() {
  pinConfiguration_statusLed = 12;
  pinConfiguration_relay = 11;
  pinConfiguration_encoderButton = 3;
  pinConfiguration_encoderA = 2;
  pinConfiguration_encoderB = 0;
  pinConfiguration_backButton = 7;
  pinConfiguration_displayDS = 5;
  pinConfiguration_displaySTCP = 13;
  pinConfiguration_displaySHCP = A0;
  pinConfiguration_displayD1 = 6;
  pinConfiguration_displayD2 = 9;
  pinConfiguration_displayD3 = 10;
  pinConfiguration_displayDL = 8;
  pinConfiguration_beep = A5;
  pinConfiguration_thermocoupleCS = 1;
  pinConfiguration_thermocoupleSCK = SCK;
  pinConfiguration_thermocoupleMISO = MISO;
  MAX_ALLOWABLE_INTERNAL = 55; //degrees C; rough limit to protect Reflowster
}

void Reflowster::init() {
  status = new Adafruit_NeoPixel(1, pinConfiguration_statusLed, NEO_GRB + NEO_KHZ800);
  knob = new Encoder(pinConfiguration_encoderB, pinConfiguration_encoderA);
  probe = new Adafruit_MAX31855(pinConfiguration_thermocoupleSCK,pinConfiguration_thermocoupleCS,pinConfiguration_thermocoupleMISO);
  display = new ReflowDisplay(pinConfiguration_displayDS,pinConfiguration_displaySTCP,pinConfiguration_displaySHCP,pinConfiguration_displayD1,pinConfiguration_displayD2,pinConfiguration_displayD3,pinConfiguration_displayDL);
  
  status->begin();
  status->show();
  
  pinMode(pinConfiguration_thermocoupleCS, OUTPUT);
  pinMode(pinConfiguration_relay, OUTPUT);  
  pinMode(pinConfiguration_backButton, INPUT_PULLUP);
  pinMode(pinConfiguration_encoderButton, INPUT_PULLUP);
  pinMode(pinConfiguration_encoderA, INPUT_PULLUP);
  pinMode(pinConfiguration_encoderB, INPUT_PULLUP);
  pinMode(pinConfiguration_displayDS, OUTPUT);
  pinMode(pinConfiguration_displaySTCP, OUTPUT);
  pinMode(pinConfiguration_displaySHCP, OUTPUT);
  pinMode(pinConfiguration_beep, OUTPUT);
  
  pulseStatus = -1;
  
//  while(1) {
//    Serial.print("pos: ");
//    Serial.println(knob->read());
//    if (getButton()) {
//      knob->write(0);
//    }
//    delay(20);
//  }
}

void Reflowster::selfTest() {
  display->displayMarquee("Testing");
  
  beep(200,150);
  delay(150);
  beep(230,150);
  delay(150); 
  
  display->setSegments(0b11111111,0b11111111,0b11111111,0b11111111);
  while(!getButton());
  
  setStatusColor(25,0,0);
  delay(200);
  setStatusColor(0,25,0);
  delay(200);
  setStatusColor(0,0,25);
  delay(200);
  setStatusColor(0,0,0);
  
  display->display("bck");
  while(!getBackButton());
  
  relayOn();
  delay(500);
  relayOff();
  
  int pknob = 5;
  setKnobPosition(pknob);
  boolean up = false;
  boolean down = false;
  while(!getButton()) {
    if (getKnobPosition() > pknob) up = true;
    if (getKnobPosition() < pknob) down = true;
    pknob = getKnobPosition();
    display->display(pknob);
    if (!up && !down) {
      setStatusColor(25,0,0);
    } else if (up && down) {
      setStatusColor(0,25,0);
    } else if (up || down) {
      setStatusColor(25,25,0);
    }
    delay(100);
  }
  setStatusColor(0,0,0);
  
  while(getButton()); //debounce
  delay(500);
  
  double temp;
  while(!getButton()) {
    temp = readCelsius();
    if (isnan(temp)) {
      display->display("err");
    } else {
      display->display(temp);
    }
    delay(200);
  }
  
  beep(200,150);
  delay(150);
  beep(230,150);
  delay(150);
  
  display->clear();
}

void Reflowster::tick() {
  display->tick();
	pulseTick();
}

long lastPulseCall = millis();
void Reflowster::pulseTick() {
  if (millis() - lastPulseCall > 50) {
    handlePulse();
    lastPulseCall = millis();
  }
}

// Tone
/////////
void Reflowster::beep(int freq, int duration) {
   tone(pinConfiguration_beep,freq,duration); 
}

// Status LED
/////////
void Reflowster::setStatusColor(byte r, byte g, byte b) {
  pulseStatus = -1;
  status->setBrightness(255);
  status->setPixelColor(0, status->Color(r,g,b));
  status->show(); 
}

void Reflowster::setStatusPulse(byte r, byte g, byte b) {
  pulseStatus = 0;
  pulseColor[0] = r;
  pulseColor[1] = g;
  pulseColor[2] = b;
}

#define PULSE_DURATION 20
#define PULSE_MIN 0.2
void Reflowster::handlePulse() {
  if (pulseStatus != -1) {
    float currentPulseMagnitude = (float)(pulseStatus > PULSE_DURATION ? 2*PULSE_DURATION - pulseStatus : pulseStatus) / (float)PULSE_DURATION;
    float ledMultiplier = PULSE_MIN + (1.0-PULSE_MIN)*currentPulseMagnitude;
    pulseStatus = (pulseStatus + 1);
    if (pulseStatus > PULSE_DURATION*2) pulseStatus = 0;
    status->setPixelColor(0, status->Color((int)(ledMultiplier*pulseColor[0]),(int)(ledMultiplier*pulseColor[1]),(int)(ledMultiplier*pulseColor[2])));
    status->show();
  }
}

// Display
//////////
ReflowDisplay * Reflowster::getDisplay() {
  return display;
}

void Reflowster::displayTest() {
  int i,l;
	byte testSegments[8] = {0b00000010,0b01000000,0b00100000,0b00010000,0b00001000,0b00000100,0b00000001,0b10000000};
  for (l=0; l<8; l++) {
    for (i=0; i<4; i++) {
      display->setSegment(testSegments[l],i);
    }
    delay(50);
  }
  //display->display(888);
	setStatusColor(20,0,0);
	delay(70);
	setStatusColor(0,20,0);
	delay(70);
	setStatusColor(0,0,20);
	delay(70);
	setStatusColor(0,0,0);
}

// Buttons
//////////
boolean Reflowster::getBackButton() {
  return !digitalRead(pinConfiguration_backButton);
}

boolean Reflowster::getButton() {
  return !digitalRead(pinConfiguration_encoderButton);
}

// Encoder
//////////
void Reflowster::setKnobPosition(int val) {
//    Serial.print("raw write: ");
//    Serial.println(val);
    knob->write(val << 2); 
}

int Reflowster::getKnobPosition() {
    long encPosition = knob->read();
//    Serial.print("raw read: ");
//    Serial.println(encPosition);
    int enc = (int)(encPosition >> 2);
    return enc;
}

// Thermocouple
///////////////
double Reflowster::readCelsius() {
  return probe->readCelsius();
}

double Reflowster::readFahrenheit() {
  return probe->readFarenheit(); //note, the misspelling in the adafruit library
}

double Reflowster::readInternalC() {
  return probe->readInternal(); //note, the misspelling in the adafruit library
}

// Relay
////////
void Reflowster::relayOn() { //can we add a return to this one to indicate if it worked?
  double temp = readInternalC();
	if (temp <= (MAX_ALLOWABLE_INTERNAL-5)) { //overheat protection prohibits relay from coming on; degrees C
		digitalWrite(pinConfiguration_relay,HIGH);
    Serial.println("Relay ON");
  } else {
	  Serial.print("Error: Relay Locked Off; Too Hot ");
		Serial.print(temp);
		Serial.println("C");
	}
}

void Reflowster::relayOff() {
  digitalWrite(pinConfiguration_relay,LOW);
  Serial.println("Relay OFF");
}

void Reflowster::relayToggle() {
  if (digitalRead(pinConfiguration_relay)) {
    relayOff();
  } else {
    relayOn();    
  }
}

boolean Reflowster::relayStatus() {
  return digitalRead(pinConfiguration_relay);
}
