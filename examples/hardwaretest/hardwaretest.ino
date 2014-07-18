#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Encoder.h>
#include "Adafruit_MAX31855.h"

#include <string.h>
#include <EEPROM.h>
#include "Reflowster.h"

const int REVISION=0;

/*
go
set profile
	pb
	pb free
	custom
		soak temp
		soak duration
		peak temperature
		save
monitor
*/

Reflowster reflowster;

struct profile {
  profile() : soakTemp(), soakTime(), peakTemp() {}
  profile(int stp, int sti, int ptp) : soakTemp( stp ), soakTime( sti ), peakTemp( ptp ) {} 
  byte soakTemp;
  byte soakTime;
  byte peakTemp;
};

profile profile_min(0,0,0);
profile profile_max(250,250,250);

profile leaded, unleaded, custom;
byte activeProfile;

int activeCommand = 0;
// This represents the currently active command. When a command is received, this is triggered and the busy loops handle
// adjusting the state to execute the command
const int CMD_REFLOW_START=1;
const int CMD_REFLOW_STOP=2;

// This represents the current state that the reflowster is in, it is used by the command processing framework
// It should be considered read only in most cases
int activeMode = 0;
const int MODE_REFLOW=1;
const int MODE_MENU=2;

const int CONFIG_LOCATION = 200;
const int CONFIG_SELF_TEST = 0;
const int CONFIG_TEMP_MODE = 1;

const int TEMP_MODE_C = 0;
const int TEMP_MODE_F = 1;
const int DEFAULT_TEMP_MODE = TEMP_MODE_C;

void setup() {
  Serial.begin(9600);

  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 65500;            // preload timer 65536-16MHz/256/2Hz
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();
  
  reflowster.init();
  
  //We run a self test the first time the unit is powered on.. when it passes, the self test is never run again
  if (readConfig(CONFIG_SELF_TEST) != 0) {
    reflowster.selfTest();
    reflowster.getDisplay()->display("pas");
    while(1) {
      if (reflowster.getButton()) {
        writeConfig(CONFIG_SELF_TEST,0);
        break;
      }
      
      if (reflowster.getBackButton()) break;
    }
  }

  reflowster.displayTest();
  reflowster.getDisplay()->display(REVISION);
  delay(500);
  loadProfiles();
}

unsigned long lastService = millis();
ISR(TIMER1_OVF_vect) {
  TCNT1 = 65500;
  
  if (millis() - lastService > 1) {
    reflowster.tick();
    lastService = millis();
  }
  processCommands();
}

byte debounceButton(int b) {
  if (!digitalRead(b)) {
    while(!digitalRead(b));
    delay(100);
    return 1;
  }
  return 0;
}

void processCommands() {
  char buffer[30];
  boolean recognized = false;
  char i = 0;
  while (Serial.available()) {
    buffer[i++] = Serial.read();
  }
  if (i != 0) {
    buffer[i] = 0;
    String command = String(buffer);
    int spaceAt = command.indexOf(" ");
    String arguments = command.substring(spaceAt+1);
    if (spaceAt == -1) arguments = "";
    Serial.write("> ");
    Serial.write(buffer);
    Serial.write("\n");
    if (activeMode == MODE_MENU) {
      if (command.startsWith("relay ")) {
        if (arguments.equalsIgnoreCase("on")) {
          recognized = true;
          reflowster.relayOn();
        } else if (arguments.equalsIgnoreCase("off")) {
          recognized = true;
          reflowster.relayOff();        
        } else if (arguments.equalsIgnoreCase("toggle")) {
          recognized = true;
          reflowster.relayToggle();
        }
      } else if (command.startsWith("setst ")) {
        //TODO dedup this code by taking advantage of the indexing in a soldering profile struct
        recognized = true;
        int val = arguments.toInt();
        if (val < profile_min.soakTemp || val > profile_max.soakTemp) {
          Serial.println("Temperature out of range!");
        } else {
          custom.soakTemp = (byte)val;
          saveProfile(1, &custom);
          Serial.print("Set soak temperature: ");
          Serial.println(custom.soakTemp);
        }
      } else if (command.startsWith("setsd ")) {
        recognized = true;
        int val = arguments.toInt();
        if (val < profile_min.soakTime || val > profile_max.soakTime) {
          Serial.println("Time out of range!");
        } else {
          custom.soakTime = (byte)val;
          saveProfile(1, &custom);
          Serial.print("Set soak time: ");
          Serial.println(custom.soakTime);
        }
      } else if (command.startsWith("setpt ")) {
        recognized = true;
        int val = arguments.toInt();
        if (val < profile_min.peakTemp || val > profile_max.peakTemp) {
          Serial.println("Temperature out of range!");
        } else {
          custom.peakTemp = (byte)val;
          saveProfile(1, &custom);
          Serial.print("Set peak temp: ");
          Serial.println(custom.peakTemp);
        }
      } else if (command.equalsIgnoreCase("start")) {
        recognized = true;
        activeCommand = CMD_REFLOW_START;
      }
    } else if (activeMode == MODE_REFLOW) {
      if (command.equalsIgnoreCase("stop")) {
        recognized = true;
        activeCommand = CMD_REFLOW_STOP;
      }
    }
    
    if (command.equalsIgnoreCase("status")) {
      recognized = true;
      if (activeMode == MODE_MENU) {
        Serial.println("Status: menus");
      } else if (activeMode == MODE_REFLOW) {
        Serial.println("Status: reflow in progress");        
      }
      Serial.print("Current thermocouple reading (C): ");
      Serial.println(reflowster.readCelsius());
      
      Serial.println();
      

      struct profile * active = &leaded;
      if (activeProfile == 1) active = &unleaded;
      if (activeProfile == 2) active = &custom;

      if (active == &leaded) Serial.println("Configuration: leaded");
      if (active == &unleaded) Serial.println("Configuration: unleaded");
      if (active == &custom) Serial.println("Configuration: custom");
      Serial.print("Soak Temperature: ");
      Serial.println(active->soakTemp);
      
      Serial.print("Soak Time: ");
      Serial.println(active->soakTime);
      
      Serial.print("Peak Temperature: ");
      Serial.println(active->peakTemp);
    } else if (command.equalsIgnoreCase("help")) {
      Serial.println("Reflowster accepts the following commands in normal mode:");
      Serial.println("relay on|off|toggle, setst deg_c, setsd time_s, setpt deg_c, start, status, help");
      Serial.println();
      Serial.println("Reflowster accepts the following commands during a reflow:");
      Serial.println("stop, status, help");
    }
    if (recognized == false) {
      Serial.println("Unrecognized or invalid command!");
    }
  }
}

int displayMenu(char * options[], int len, int defaultChoice) {
  activeMode = MODE_MENU;
  int menuIndex = -1;
  reflowster.setKnobPosition(defaultChoice);
  while(1) {
    reflowster.pulseTick();
    
    if (activeCommand != 0) return -1;
    if (debounceButton(reflowster.pinConfiguration_encoderButton)) {
      reflowster.getDisplay()->clear();
      return menuIndex;
    }
    if (debounceButton(reflowster.pinConfiguration_backButton)) {
      reflowster.getDisplay()->clear();
      return -1;
    }
    
    int newIndex = reflowster.getKnobPosition();
    Serial.println();
    Serial.print("oldIndex: ");
    Serial.println(menuIndex);
    Serial.print("newIndex: ");
    Serial.println(newIndex);
    
    if (newIndex >= len) {
      newIndex = len - 1;
      Serial.print("setting knob: ");
      Serial.println(newIndex);
      reflowster.setKnobPosition(newIndex);
    }
    
    if (newIndex < 0) {
      newIndex = 0;
      Serial.print("setting knob: ");
      Serial.println(newIndex);
      reflowster.setKnobPosition(newIndex);
    }
    
    if (newIndex != menuIndex) {
      reflowster.beep(400,20);
      menuIndex = newIndex;
      reflowster.getDisplay()->displayMarquee(options[menuIndex]);
    }

    delay(100);
  }
  reflowster.getDisplay()->clear();
  return menuIndex;
}

int chooseNum(int low, int high, int defaultVal) {
  int val = defaultVal;
  reflowster.setKnobPosition(val);
  while(1) {
    if (debounceButton(reflowster.pinConfiguration_encoderButton)) return val;
    if (debounceButton(reflowster.pinConfiguration_backButton)) return defaultVal;
    
    val = reflowster.getKnobPosition();
    if (val > high) {
      val = high;
      reflowster.setKnobPosition(val);
    }
    if (val < low) {
      val = low;
      reflowster.setKnobPosition(val);
    }
    
    reflowster.getDisplay()->display(val);
    delay(100);
  }
}

void loadProfiles() {
  leaded.soakTemp = 100;
  leaded.soakTime = 90;
  leaded.peakTemp = 190;
  
  unleaded.soakTemp = 100;
  unleaded.soakTime = 90;
  unleaded.peakTemp = 190;

  custom.soakTemp = leaded.soakTemp;
  custom.soakTime = leaded.soakTime;
  custom.peakTemp = custom.peakTemp;
  
  activeProfile = EEPROM.read(0); //read the selected profile
  if (activeProfile > 2) activeProfile = 0;

  loadProfile(1,&custom); //use the leaded profile as the default
}

void loadProfile(byte loc, struct profile * target) {
  for (byte i=0; i<3; i++) {
    byte val = EEPROM.read(loc+i);
    if (val != 255) *(((byte*)target)+i) = val; //pointer-fu to populate the profile struct
  }
}

void setActiveProfile(byte p) {
  activeProfile = p;
  if (EEPROM.read(0) != activeProfile) ewrite(0,activeProfile);
}

void saveProfile(byte loc, struct profile * target) {
  for (byte i=0; i<3; i++) {
    byte val = EEPROM.read(loc+i);
    if (val != *(((byte*)target)+i)) ewrite(loc+i,*(((byte*)target)+i)); //we only write to eeprom if the value is changed
  }
}

void writeConfig(int cfg, byte value) {
  ewrite(CONFIG_LOCATION+cfg,value);
}

byte readConfig(int cfg) {
  return EEPROM.read(CONFIG_LOCATION+cfg);
}

void ewrite(byte loc, byte val) {
  /*
  Serial.print("EEPROM WRITE ");
  Serial.print(val);
  Serial.print(" to ");
  Serial.println(loc);
  */
  EEPROM.write(loc,val);
}

void loop() { 
  mainMenu();
}

char * mainMenuItems[] = {"go","set profile","monitor","config"};
const int MAIN_MENU_SIZE = 4;
void mainMenu() {
  byte lastChoice = 0;
  while(1) {
    if (activeCommand == CMD_REFLOW_START) {
      activeCommand = 0;
      doReflow();
    }
    int choice = displayMenu(mainMenuItems,MAIN_MENU_SIZE,lastChoice);
    if (choice != -1) lastChoice = choice;
    switch(choice) {
      case 0: doReflow(); break;

      case 1: 
        if (setProfile()) lastChoice = 0;
      break;

      case 2: doMonitor(); break;
      
      case 3: configMenu(); break;
    }
  }
}

void dataCollectionMode() {
  double goalTemp = 80;
  unsigned long lastReport = millis();
  
  unsigned long lastCycleStart = millis();
  float ratio = .1;
  int timeUnit = 30000;
  
  long REPORT_FREQUENCY = 5000;
  while(1) {
    double temp = 0;
    if (readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_F) {
      temp = reflowster.readFahrenheit();
    } else {
      temp = reflowster.readCelsius();
    }
    if ((millis() - lastReport) > REPORT_FREQUENCY) {
      Serial.print("data: ");
      Serial.print(temp);
      Serial.print(" ");
      Serial.println(reflowster.relayStatus());
      lastReport += REPORT_FREQUENCY;
    }
    
    long current = millis();
    long cycleDuration = current - lastCycleStart;
    if (current > lastCycleStart+timeUnit) {
      lastCycleStart += timeUnit;
      cycleDuration = 0;
    } 
    double cratio = (double)cycleDuration/(double)timeUnit;
    boolean on = cratio < ratio;
    if (temp < 88) on = true;
//    Serial.print("cycleDuration: ");
//    Serial.println(cycleDuration);
//    Serial.print("cratio: ");
//    Serial.println(cratio);
    if (reflowster.relayStatus() && !on) {
      reflowster.relayOff();
    } else if (!reflowster.relayStatus() && on) {
      reflowster.relayOn();        
    }
    
    if (reflowster.getBackButton()) {
       while(reflowster.getBackButton());
       delay(1000);
       return; 
    }
    
    delay(300);
  }
}

void doReflow() {
//  dataCollectionMode();
//  return;
  if (isnan(reflowster.readCelsius())) {
    reflowster.getDisplay()->displayMarquee("err no temp");
    Serial.println("Error: Thermocouple could not be read, check connection!");
    while(!reflowster.getDisplay()->marqueeComplete());
    return;
  }
  struct profile * active = &leaded;
  if (activeProfile == 1) active = &unleaded;
  if (activeProfile == 2) active = &custom;

  byte soakTemp = active->soakTemp;
  byte soakTime = active->soakTime;
  byte peakTemp = active->peakTemp;

  activeMode = MODE_REFLOW;
  if (reflowImpl(soakTemp,soakTime,peakTemp) == 0) {  
    reflowster.getDisplay()->displayMarquee("done");
  } else {
    reflowster.getDisplay()->displayMarquee("cancelled");    
  }
  while(!reflowster.getDisplay()->marqueeComplete());
}

char * profileMenuItems[] = {"+pb leaded","-pb unleaded","custom"};
boolean setProfile() {
  while(1) {
    int choice = displayMenu(profileMenuItems,3,choice);
    switch(choice) {
      case -1: return false;
      case 0:
        setActiveProfile(choice);
        return true;
      break;
  
      case 1:
        setActiveProfile(choice);
        return true;
      break;
  
      case 2:
        if (editCustomProfile()) {
          setActiveProfile(choice);
          return true;
        }
      break;
    }
  }
}

char * editProfileMenuItems[] = {"st-soak temp","sd-soak duration","pt-peak temp","set"};
boolean editCustomProfile() {
  int choice = 0;
  int val;
  byte celsius;
  while(1) {
    choice = displayMenu(editProfileMenuItems,4,choice);
    switch(choice) {
      case -1: return false;
      case 0:
      case 1:
      case 2:
        celsius = *(((byte*)&custom)+choice);
        val = readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_C ? celsius : ctof(celsius);
        val = chooseNum(0,readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_C ? 255 : ctof(255),val);
        val = readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_C ? val : ftoc(val);
        *(((byte*)&custom)+choice) = val;

        saveProfile(1,&custom);
      break;

      case 3:
        return true;
    }
  }
}

void doMonitor() {
  unsigned long lastReport = millis();
  int MONITOR_FREQUENCY = 1000;
  while(1) {
    
    double temp;
    if (readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_F) {
      temp = reflowster.readFahrenheit();
    } else {
      temp = reflowster.readCelsius();
    }
    reflowster.getDisplay()->display((int)temp);

    if ((millis() - lastReport) > MONITOR_FREQUENCY) {  //generate a 1000ms event period
      Serial.println(temp);
      lastReport += MONITOR_FREQUENCY;
    }
    
    if (debounceButton(reflowster.pinConfiguration_encoderButton)) return;
    if (debounceButton(reflowster.pinConfiguration_backButton)) return;

    delay(50);
  } 
}

char * configMenuItems[] = {"temp mode"};
const int CONFIG_MENU_ITEMS = 1;

char * tempModeMenu[] = {"Cel","Fah"};
const int TEMP_MODE_ITEMS = 2;
void configMenu() {
  int choice = 0;
  while(1) {
    choice = displayMenu(configMenuItems,CONFIG_MENU_ITEMS,choice);
    switch(choice) {
      case -1: return;
      case 0:
        int tempChoice = readConfig(CONFIG_TEMP_MODE);
        if (tempChoice == 255) tempChoice = DEFAULT_TEMP_MODE;
        tempChoice = displayMenu(tempModeMenu,TEMP_MODE_ITEMS,tempChoice);
        if (tempChoice != -1 && tempChoice != readConfig(CONFIG_TEMP_MODE)) {
          writeConfig(CONFIG_TEMP_MODE,tempChoice);
        }
      break;
    }
  }  
}

double ctof(double c) {
  return ((c*9.0)/5.0)+32.0;
}

double ftoc(double f) {
  return ((f-32.0)*5.0)/9.0;
}

double celsiusToFahrenheitIfNecessary(double c) {
  if (readConfig(CONFIG_TEMP_MODE) == TEMP_MODE_C) return c;
  return ctof(c);
}

#define PHASE_PRE_SOAK 0
#define PHASE_SOAK 1
#define PHASE_SPIKE 2
#define PHASE_COOL 3
#define CANCEL_TIME 5000
byte reflowImpl(byte soakTemp, byte soakTime, byte peakTemp) {
  unsigned long startTime = millis();
  unsigned long phaseStartTime = millis();
  unsigned long buttonStartTime = 0;
  unsigned long lastReport = millis();
  int phase = PHASE_PRE_SOAK;
  unsigned long MAXIMUM_OVEN_PHASE_TIME = 8*60; //if the oven is on for 8 minutes and we havent hit the desired tempp

  Serial.println("Starting Reflow: ");
  Serial.print("Soak Temp: ");
  Serial.println(soakTemp);
  Serial.print("Soak Time: ");
  Serial.println(soakTime);
  Serial.print("Peak Temp: ");
  Serial.println(peakTemp);
  
  int REPORT_INTERVAL = 200;
  
  reflowster.relayOn();
  byte pulseColors = 0;
  int pulse = 0;
  while(1) {
    reflowster.pulseTick();
    delay(50);
    double temp = reflowster.readCelsius();
    unsigned long currentPhaseSeconds = (millis() - phaseStartTime) / 1000;
    
    if ((millis() - lastReport) > REPORT_INTERVAL) {  //generate a 1000ms event period
      Serial.print("data: ");
      Serial.print(temp);
      Serial.print(" ");
      Serial.println(reflowster.relayStatus());
      lastReport += REPORT_INTERVAL;
    }
    
    if (activeCommand == CMD_REFLOW_STOP) {
        Serial.println("Reflow cancelled");
        activeCommand = 0;
        reflowster.relayOff();
        return -1;
    }
    if (buttonStartTime == 0) {
      reflowster.getDisplay()->display((int)celsiusToFahrenheitIfNecessary(temp));
      if (reflowster.getBackButton()) {
        reflowster.getDisplay()->displayMarquee("Hold to cancel");
        buttonStartTime = millis();
      }
    } else {
      if ((millis() - buttonStartTime) > CANCEL_TIME) {
        reflowster.relayOff();
        return -1;
      }
      if (!reflowster.getBackButton()) buttonStartTime = 0;
    }
    switch(phase) {
      case PHASE_PRE_SOAK: {
        reflowster.setStatusPulse(25,5,0);
        if (currentPhaseSeconds > MAXIMUM_OVEN_PHASE_TIME) {
          reflowster.relayOff();
          return -1;
        }
        if (temp >= soakTemp) {
          phase = PHASE_SOAK;
          phaseStartTime = millis();
          reflowster.relayOff();
        }
        break;
      }
      
      case PHASE_SOAK: {
        reflowster.setStatusPulse(25,15,0);
        if (currentPhaseSeconds > soakTime) {
          phase = PHASE_SPIKE;
          phaseStartTime = millis();
          reflowster.relayOn();
        }
        break;
      }
      
      case PHASE_SPIKE: {
        reflowster.setStatusPulse(25,0,0);
        if (temp >= peakTemp || currentPhaseSeconds > MAXIMUM_OVEN_PHASE_TIME) {
          phase = PHASE_COOL;
          phaseStartTime = millis();
          reflowster.relayOff();
        }
        break;
      }
      
      case PHASE_COOL: {
        reflowster.setStatusPulse(0,0,25);
        unsigned long currentCoolSeconds = (millis() - phaseStartTime) / 1000;
        if (debounceButton(reflowster.pinConfiguration_backButton) || debounceButton(reflowster.pinConfiguration_encoderButton)) {
//        if (currentCoolSeconds > 30 || temp < 60 || debounceButton(reflowster.pinConfiguration_backButton) || debounceButton(reflowster.pinConfiguration_encoderButton)) {
          reflowster.relayOff();
          return 0;
        }
        break;
      }
    }
  }
}
