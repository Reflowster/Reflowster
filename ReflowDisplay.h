#ifndef ReflowDisplay_h
#define ReflowDisplay_h

#include "Arduino.h"

class ReflowDisplay {
public:
  ReflowDisplay(int DS, int STCP, int SHCP, int D1, int D2, int D3, int DL); //setup constructor

  int marqueeStartWait;
  int marqueeEndWait;
  
  void display(int n);
  void display(char * s);
  void displayMarquee(char * chars);
  boolean marqueeComplete();
  void setSegment(byte segment, byte index);
  void setSegments(byte a, byte b, byte c, byte d);
  void tick();
  void clear();
  
  
private:
  int pinConfiguration_DS;
  int pinConfiguration_STCP;
  int pinConfiguration_SHCP;
  int pinConfiguration_D1;
  int pinConfiguration_D2;
  int pinConfiguration_D3;
  int pinConfiguration_DL;

  int tickCounter;
  unsigned long marqueeTimer;
  byte displayedDigits[4];
  char * marqueeString;
  int marqueeLength;
  int marqueeIndex;
  boolean marqueeCompleteFlag;
  
  byte getLetter(char a);
  void displayChars(char * chars, int len);
  void stopMarquee();
  void marqueeHandler();
  void displayDigit(byte segments, byte displayDigit);
  
  static byte numerals[];
  static byte alphabet[];
};

#endif
