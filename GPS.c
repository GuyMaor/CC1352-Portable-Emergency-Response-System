/***********************************
This is our GPS library

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.
BSD license, check license.txt for more information
All text above must be included in any redistribution
****************************************/

#include "GPS.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/GPIO.h>
#include "Board.h"

// how long are max NMEA lines to parse?
#define MAXLINELENGTH 120

// we double buffer: read one line in and leave one for the main program
volatile char line1[MAXLINELENGTH];
volatile char line2[MAXLINELENGTH];
// our index into filling the current line
volatile uint8_t lineidx=0;
// pointers to the double buffers
volatile char *currentline;
volatile char *lastline;
volatile bool recvdflag;
volatile bool inStandbyMode;

UART_Handle uart;
UART_Params uartParams;
char data[1000];
static void readCallback1(UART_Handle handle, void *rxBuf, int size)
{
    // Do nothing
}

// Write callback function
static void writeCallback1(UART_Handle handle, void *rxBuf, int size)
{
    // Do nothing
}

void DelayMsG(uint32_t n){
  volatile uint32_t time;
  while(n){
    time = 4000;  // 1msec, tuned at 48 MHz
    while(time){
      time--;
    }
    n--;
  }
}

void get_loc(char *info) {
    UART_read(uart, &data[0], 400);
    DelayMsG(1000);
    UART_readCancel(uart);
    UART_read(uart, &data[0], 400);
    DelayMsG(1000);
    UART_readCancel(uart);
    UART_read(uart, &data[0], 400);
    DelayMsG(1000);
    UART_readCancel(uart);
    int i;
    info[0]='T';
    info[1]=':';
    for(i = 0; i<6; i++) {
        info[2+i] = data[138+i];
    }
    info[8] ='%';
    info[9] ='2';
    info[10] ='0';
    info[11] ='N';
    info[12] =':';
    for(i = 0; i<9; i++) {
        info[13+i] = data[151+i];
    }
    info[22] ='%';
    info[23] ='2';
    info[24] ='0';
    info[25] ='W';
    info[26] =':';
    for(i = 0; i<10; i++) {
        info[27+i] = data[163+i];
    }
    info[37] ='%';
    info[38] ='2';
    info[39] ='0';
    info[40] ='D';
    info[41] =':';
    info[42] = data[190];
    info[43] = data[191];
    info[44] ='.';
    info[45] = data[188];
    info[46] = data[189];
    info[47] ='.';
    info[48] = data[192];
    info[49] = data[193];
    info[50] ='%';
    info[51] ='2';
    info[52] ='0';
    info[53] ='I';
    info[54] ='D';
    info[55] =':';
    info[56] ='9';
    info[57] ='9';
    info[58] ='9';
    info[59] ='9';
    info[60] ='%';
    info[61] ='2';
    info[62] ='0';
    info[63] ='J';
    info[64] ='u';
    info[65] ='m';
    info[66] ='p';
    info[67] ='s';
    info[68] =':';
    info[69] ='9';
    info[70] ='9';
    info[71] = 0;
    printf("GPS: %s", data);
    int k;
    int start = 0;
    for(k = 0; k<500; k++){
        if(data[k]=='G' && data[k+1]='P' && data[k+2]=='R' && data[k+3]='M' && data[k+4]=='C') {
           start = 1;
        }
        if(start){
            for(j =0;j<70;j++){
                info[j]= data[k+j];
            }
            info[71]=0;
            return;
        }
    }
}

bool GPS_Init(void) {
    UART_Params_init(&uartParams);
    uartParams.baudRate      = 9600;
    uartParams.writeMode     = UART_MODE_BLOCKING;
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.writeCallback = writeCallback1;
    uartParams.writeTimeout   = 10000000;
    uartParams.readMode      = UART_MODE_CALLBACK;
    uartParams.readDataMode  = UART_DATA_BINARY;
    uartParams.readCallback  = readCallback1;
    uartParams.readTimeout   = 500000;
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        printf("UART_open() failed!\n\r");
        //while(1){};
        return 0;
    }
    UART_write(uart, PMTK_SET_NMEA_OUTPUT_RMCGGA, sizeof(PMTK_SET_NMEA_OUTPUT_RMCGGA));
    UART_write(uart, PMTK_SET_NMEA_UPDATE_5HZ, sizeof(PMTK_SET_NMEA_UPDATE_5HZ));
    UART_write(uart, PMTK_API_SET_FIX_CTL_5HZ, sizeof(PMTK_API_SET_FIX_CTL_5HZ));
    return 1;
}

bool parse(char *nmea) {
  // do checksum check

  // first look if we even have one
  if (nmea[strlen(nmea)-4] == '*') {
    uint16_t sum = parseHex(nmea[strlen(nmea)-3]) * 16;
    sum += parseHex(nmea[strlen(nmea)-2]);

    // check checksum
    uint8_t i;
    for (i=2; i < (strlen(nmea)-4); i++) {
      sum ^= nmea[i];
    }
    if (sum != 0) {
      // bad checksum :(
      return false;
    }
  }
  int32_t degree;
  long minutes;
  char degreebuff[10];
  // look for a few common sentences
  if (strstr(nmea, "$GPGGA")) {
    // found GGA
    char *p = nmea;
    // get time
    p = strchr(p, ',')+1;
    float timef = atof(p);
    uint32_t time = timef;
    hour = time / 10000;
    minute = (time % 10000) / 100;
    seconds = (time % 100);

    milliseconds = fmod(timef, 1.0) * 1000;

    // parse out latitude
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      strncpy(degreebuff, p, 2);
      p += 2;
      degreebuff[2] = '\0';
      degree = atol(degreebuff) * 10000000;
      strncpy(degreebuff, p, 2); // minutes
      p += 3; // skip decimal point
      strncpy(degreebuff + 2, p, 4);
      degreebuff[6] = '\0';
      minutes = 50 * atol(degreebuff) / 3;
      latitude_fixed = degree + minutes;
      latitude = degree / 100000 + minutes * 0.000006F;
      latitudeDegrees = (latitude-100*(int)(latitude/100))/60.0;
      latitudeDegrees += (int)(latitude/100);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      if (p[0] == 'S') latitudeDegrees *= -1.0;
      if (p[0] == 'N') lat = 'N';
      else if (p[0] == 'S') lat = 'S';
      else if (p[0] == ',') lat = 0;
      else return false;
    }

    // parse out longitude
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      strncpy(degreebuff, p, 3);
      p += 3;
      degreebuff[3] = '\0';
      degree = atol(degreebuff) * 10000000;
      strncpy(degreebuff, p, 2); // minutes
      p += 3; // skip decimal point
      strncpy(degreebuff + 2, p, 4);
      degreebuff[6] = '\0';
      minutes = 50 * atol(degreebuff) / 3;
      longitude_fixed = degree + minutes;
      longitude = degree / 100000 + minutes * 0.000006F;
      longitudeDegrees = (longitude-100*(int)(longitude/100))/60.0;
      longitudeDegrees += (int)(longitude/100);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      if (p[0] == 'W') longitudeDegrees *= -1.0;
      if (p[0] == 'W') lon = 'W';
      else if (p[0] == 'E') lon = 'E';
      else if (p[0] == ',') lon = 0;
      else return false;
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      fixquality = atoi(p);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      satellites = atoi(p);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      HDOP = atof(p);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      altitude = atof(p);
    }

    p = strchr(p, ',')+1;
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      geoidheight = atof(p);
    }
    return true;
  }
  if (strstr(nmea, "$GPRMC")) {
   // found RMC
    char *p = nmea;

    // get time
    p = strchr(p, ',')+1;
    float timef = atof(p);
    uint32_t time = timef;
    hour = time / 10000;
    minute = (time % 10000) / 100;
    seconds = (time % 100);

    milliseconds = fmod(timef, 1.0) * 1000;

    p = strchr(p, ',')+1;
    // Serial.println(p);
    if (p[0] == 'A')
      fix = true;
    else if (p[0] == 'V')
      fix = false;
    else
      return false;

    // parse out latitude
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      strncpy(degreebuff, p, 2);
      p += 2;
      degreebuff[2] = '\0';
      long degree = atol(degreebuff) * 10000000;
      strncpy(degreebuff, p, 2); // minutes
      p += 3; // skip decimal point
      strncpy(degreebuff + 2, p, 4);
      degreebuff[6] = '\0';
      long minutes = 50 * atol(degreebuff) / 3;
      latitude_fixed = degree + minutes;
      latitude = degree / 100000 + minutes * 0.000006F;
      latitudeDegrees = (latitude-100*(int)(latitude/100))/60.0;
      latitudeDegrees += (int)(latitude/100);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      if (p[0] == 'S') latitudeDegrees *= -1.0;
      if (p[0] == 'N') lat = 'N';
      else if (p[0] == 'S') lat = 'S';
      else if (p[0] == ',') lat = 0;
      else return false;
    }

    // parse out longitude
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      strncpy(degreebuff, p, 3);
      p += 3;
      degreebuff[3] = '\0';
      degree = atol(degreebuff) * 10000000;
      strncpy(degreebuff, p, 2); // minutes
      p += 3; // skip decimal point
      strncpy(degreebuff + 2, p, 4);
      degreebuff[6] = '\0';
      minutes = 50 * atol(degreebuff) / 3;
      longitude_fixed = degree + minutes;
      longitude = degree / 100000 + minutes * 0.000006F;
      longitudeDegrees = (longitude-100*(int)(longitude/100))/60.0;
      longitudeDegrees += (int)(longitude/100);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      if (p[0] == 'W') longitudeDegrees *= -1.0;
      if (p[0] == 'W') lon = 'W';
      else if (p[0] == 'E') lon = 'E';
      else if (p[0] == ',') lon = 0;
      else return false;
    }
    // speed
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      speed = atof(p);
    }

    // angle
    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      angle = atof(p);
    }

    p = strchr(p, ',')+1;
    if (',' != *p)
    {
      uint32_t fulldate = atof(p);
      day = fulldate / 10000;
      month = (fulldate % 10000) / 100;
      year = (fulldate % 100);
    }
    // we dont parse the remaining, yet!
    return true;
  }

  return false;
}
/*
char read(void) {
  char c = 0;

  if (paused) return c;

#if defined(__AVR__) && defined(USE_SW_SERIAL)
  if(gpsSwSerial) {
    if(!gpsSwSerial->available()) return c;
    c = gpsSwSerial->read();
  } else
#endif
  {
    if(!gpsHwSerial->available()) return c;
    c = gpsHwSerial->read();
  }

  //Serial.print(c);

//  if (c == '$') {         //please don't eat the dollar sign - rdl 9/15/14
//    currentline[lineidx] = 0;
//    lineidx = 0;
//  }
  if (c == '\n') {
    currentline[lineidx] = 0;

    if (currentline == line1) {
      currentline = line2;
      lastline = line1;
    } else {
      currentline = line1;
      lastline = line2;
    }

    //Serial.println("----");
    //Serial.println((char *)lastline);
    //Serial.println("----");
    lineidx = 0;
    recvdflag = true;
  }

  currentline[lineidx++] = c;
  if (lineidx >= MAXLINELENGTH)
    lineidx = MAXLINELENGTH-1;

  return c;
}
*/


// Initialization code used by all constructor types
void common_init(void) {

  recvdflag   = false;
  paused      = false;
  lineidx     = 0;
  currentline = line1;
  lastline    = line2;

  hour = minute = seconds = year = month = day =
    fixquality = satellites = 0; // uint8_t
  lat = lon = mag = 0; // char
  fix = false; // boolean
  milliseconds = 0; // uint16_t
  latitude = longitude = geoidheight = altitude =
    speed = angle = magvariation = HDOP = 0.0; // float
}
/*
void begin(uint32_t baud)
{
#if defined(__AVR__) && defined(USE_SW_SERIAL)
  if(gpsSwSerial)
    gpsSwSerial->begin(baud);
  else
#endif
    gpsHwSerial->begin(baud);

  delay(10);
}*/
/*
void sendCommand(const char *str) {
#if defined(__AVR__) && defined(USE_SW_SERIAL)
  if(gpsSwSerial)
    gpsSwSerial->println(str);
  else
#endif
    gpsHwSerial->println(str);
}*/

bool newNMEAreceived(void) {
  return recvdflag;
}

void pause(bool p) {
  paused = p;
}

char *lastNMEA(void) {
  recvdflag = false;
  return (char *)lastline;
}

// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
    if (c < '0')
      return 0;
    if (c <= '9')
      return c - '0';
    if (c < 'A')
       return 0;
    if (c <= 'F')
       return (c - 'A')+10;
    // if (c > 'F')
    return 0;
}

bool waitForSentence(const char *wait4me, uint8_t max) {
  char str[20];

  uint8_t i=0;
  while (i < max) {
    read();

    if (newNMEAreceived()) {
      char *nmea = lastNMEA();
      strncpy(str, nmea, 20);
      str[19] = 0;
      i++;

        if (strstr(str, wait4me))
    return true;
    }
  }

  return false;
}

bool LOCUS_StartLogger(void) {
  sendCommand(PMTK_LOCUS_STARTLOG);
  recvdflag = false;
  return waitForSentence(PMTK_LOCUS_STARTSTOPACK,10);
}

bool LOCUS_StopLogger(void) {
  sendCommand(PMTK_LOCUS_STOPLOG);
  recvdflag = false;
  return waitForSentence(PMTK_LOCUS_STARTSTOPACK,10);
}

bool LOCUS_ReadStatus(void) {
  sendCommand(PMTK_LOCUS_QUERY_STATUS);

  if (! waitForSentence("$PMTKLOG",10))
    return false;

  char *response = lastNMEA();
  uint16_t parsed[10];
  uint8_t i;

  for (i=0; i<10; i++) parsed[i] = 0xFF;

  response = strchr(response, ',');
  for (i=0; i<10; i++) {
    if (!response || (response[0] == 0) || (response[0] == '*'))
      break;
    response++;
    parsed[i]=0;
    while ((response[0] != ',') &&
       (response[0] != '*') && (response[0] != 0)) {
      parsed[i] *= 10;
      char c = response[0];
      if (isdigit(c))
        parsed[i] += c - '0';
      else
        parsed[i] = c;
      response++;
    }
  }
  LOCUS_serial = parsed[0];
  LOCUS_type = parsed[1];
  if (isalpha(parsed[2])) {
    parsed[2] = parsed[2] - 'a' + 10;
  }
  LOCUS_mode = parsed[2];
  LOCUS_config = parsed[3];
  LOCUS_interval = parsed[4];
  LOCUS_distance = parsed[5];
  LOCUS_speed = parsed[6];
  LOCUS_status = !parsed[7];
  LOCUS_records = parsed[8];
  LOCUS_percent = parsed[9];

  return true;
}

// Standby Mode Switches
bool standby(void) {
  if (inStandbyMode) {
    return false;  // Returns false if already in standby mode, so that you do not wake it up by sending commands to GPS
  }
  else {
    inStandbyMode = true;
    sendCommand(PMTK_STANDBY);
    //return waitForSentence(PMTK_STANDBY_SUCCESS);  // don't seem to be fast enough to catch the message, or something else just is not working
    return true;
  }
}

bool wakeup(void) {
  if (inStandbyMode) {
   inStandbyMode = false;
    sendCommand("");  // send byte to wake it up
    return waitForSentence(PMTK_AWAKE,10);
  }
  else {
      return false;  // Returns false if not in standby mode, nothing to wakeup
  }
}
