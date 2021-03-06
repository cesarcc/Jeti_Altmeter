/*
   -----------------------------------------------------------
                Jeti Altitude Sensor v 1.0
   -----------------------------------------------------------

    Tero Salminen RC-Thoughts.com (c) 2016 www.rc-thoughts.com

  -----------------------------------------------------------

    Simple altitude sensor for Jeti EX telemetry with cheap
    BMP180 breakout-board and Arduino Pro Mini

  -----------------------------------------------------------
    Shared under MIT-license by Tero Salminen (c) 2016
  -----------------------------------------------------------
*/

#include <EEPROM.h>
#include <stdlib.h>
#include <SoftwareSerialJeti.h>
#include <JETI_EX_SENSOR.h>
#include "Wire.h"
#include "Adafruit_BMP085.h"

Adafruit_BMP085 bmp;

#define prog_char  char PROGMEM
#define GETCHAR_TIMEOUT_ms 20

#ifndef JETI_RX
#define JETI_RX 3
#endif

#ifndef JETI_TX
#define JETI_TX 4
#endif

#define ITEMNAME_1 F("Altitude")
#define ITEMTYPE_1 F("m")
#define ITEMVAL_1 &uAltitude

#define ABOUT_1 F(" RCT Jeti Tools")
#define ABOUT_2 F("  Altitude")

SoftwareSerial JetiSerial(JETI_RX, JETI_TX);

void JetiUartInit()
{
  JetiSerial.begin(9700);
}

void JetiTransmitByte(unsigned char data, boolean setBit9)
{
  JetiSerial.set9bit = setBit9;
  JetiSerial.write(data);
  JetiSerial.set9bit = 0;
}

unsigned char JetiGetChar(void)
{
  unsigned long time = millis();
  while ( JetiSerial.available()  == 0 )
  {
    if (millis() - time >  GETCHAR_TIMEOUT_ms)
      return 0;
  }
  int read = -1;
  if (JetiSerial.available() > 0 )
  { read = JetiSerial.read();
  }
  long wait = (millis() - time) - GETCHAR_TIMEOUT_ms;
  if (wait > 0)
    delay(wait);
  return read;
}

char * floatToString(char * outstr, float value, int places, int minwidth = 0) {
  int digit;
  float tens = 0.1;
  int tenscount = 0;
  int i;
  float tempfloat = value;
  int c = 0;
  int charcount = 1;
  int extra = 0;
  float d = 0.5;
  if (value < 0)
    d *= -1.0;
  for (i = 0; i < places; i++)
    d /= 10.0;
  tempfloat +=  d;
  if (value < 0)
    tempfloat *= -1.0;
  while ((tens * 10.0) <= tempfloat) {
    tens *= 10.0;
    tenscount += 1;
  }
  if (tenscount > 0)
    charcount += tenscount;
  else
    charcount += 1;
  if (value < 0)
    charcount += 1;
  charcount += 1 + places;
  minwidth += 1;
  if (minwidth > charcount) {
    extra = minwidth - charcount;
    charcount = minwidth;
  }
  if (value < 0)
    outstr[c++] = '-';
  if (tenscount == 0)
    outstr[c++] = '0';
  for (i = 0; i < tenscount; i++) {
    digit = (int) (tempfloat / tens);
    itoa(digit, &outstr[c++], 10);
    tempfloat = tempfloat - ((float)digit * tens);
    tens /= 10.0;
  }
  if (places > 0)
    outstr[c++] = '.';
  for (i = 0; i < places; i++) {
    tempfloat *= 10.0;
    digit = (int) tempfloat;
    itoa(digit, &outstr[c++], 10);
    tempfloat = tempfloat - (float) digit;
  }
  if (extra > 0 ) {
    for (int i = 0; i < extra; i++) {
      outstr[c++] = ' ';
    }
  }
  outstr[c++] = '\0';
  return outstr;
}

JETI_Box_class JB;

unsigned char SendFrame()
{
  boolean bit9 = false;
  for (int i = 0 ; i < JB.frameSize ; i++ )
  {
    if (i == 0)
      bit9 = false;
    else if (i == JB.frameSize - 1)
      bit9 = false;
    else if (i == JB.middle_bit9)
      bit9 = false;
    else
      bit9 = true;
    JetiTransmitByte(JB.frame[i], bit9);
  }
}

uint8_t frame[10];
short value = 27;

int startAltitude = 0;
int curPressure = 0;
int curAltitude = 0;
int uLoopCount = 0;
int uAltitude = 0;

const int numReadings = 6;
int readings[numReadings];
int readIndex = 0;
int total = 0;

#define MAX_SCREEN 1
#define MAX_CONFIG 1
#define COND_LES_EQUAL 1
#define COND_MORE_EQUAL 2

void setup()
{
  Serial.begin(9600);
  analogReference(EXTERNAL);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(JETI_RX, OUTPUT);
  JetiUartInit();

  JB.JetiBox(ABOUT_1, ABOUT_2);
  JB.Init(F("RCT"));
  JB.addData(ITEMNAME_1, ITEMTYPE_1);
  JB.setValue(1, ITEMVAL_1);

  bmp.begin();
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  
  do {
    JB.createFrame(1);
    SendFrame();
    delay(GETCHAR_TIMEOUT_ms);
  }
  while (sensorFrameName != 0);
  digitalWrite(13, LOW);
}

int header = 0;
int lastbtn = 240;
int current_screen = 1;
int current_config = 0;

float alarm_current = 0;

char temp[LCDMaxPos / 2];
char msg_line1[LCDMaxPos / 2];
char msg_line2[LCDMaxPos / 2];

void loop()
{ 
  curAltitude = bmp.readAltitude();

  if (uLoopCount == 20) {
    startAltitude = curAltitude;
  }

  total = total - readings[readIndex];
  readings[readIndex] = curAltitude;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  uAltitude = (total / numReadings) - startAltitude;

  if (uLoopCount < 21) {
    uLoopCount++;
    uAltitude = 0;
  }

  //Serial.print("Altitude = "); // Uncomment these for PC debug
  //Serial.print(uAltitude);
  //Serial.println(" meters");
  //Serial.println();

  unsigned long time = millis();
  SendFrame();
  time = millis();
  int read = 0;
  pinMode(JETI_RX, INPUT);
  pinMode(JETI_TX, INPUT_PULLUP);

  JetiSerial.listen();
  JetiSerial.flush();

  while ( JetiSerial.available()  == 0 )
  {

    if (millis() - time >  5)
      break;
  }

  if (JetiSerial.available() > 0 )
  { read = JetiSerial.read();

    if (lastbtn != read)
    {
      lastbtn = read;
      switch (read)
      {
        case 224 :
          break;
        case 112 :
          break;
        case 208 :

          break;
        case 176 :
          break;
      }
    }
  }

  if (current_screen != MAX_SCREEN)
    current_config = 0;
  header++;
  if (header >= 5)
  {
    JB.createFrame(1);
    header = 0;

  }
  else
  {
    JB.createFrame(0);
  }

  long wait = GETCHAR_TIMEOUT_ms;
  long milli = millis() - time;
  if (milli > wait)
    wait = 0;
  else
    wait = wait - milli;
  pinMode(JETI_TX, OUTPUT);
}
