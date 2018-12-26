// esp8266.c
// Brief desicription of program:
// - Initializes an ESP8266 module to act as a WiFi client
//   and fetch weather data from openweathermap.org
//
/*
  Author: Steven Prickett (steven.prickett@gmail.com)

  THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
  OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
  VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
  OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.

*/
// http://www.xess.com/blog/esp8266-reflash/

// NOTE: ESP8266 resources below:
// General info and AT commands: http://nurdspace.nl/ESP8266
// General info and AT commands: http://www.electrodragon.com/w/Wi07c
// Community forum: http://www.esp8266.com/
// Offical forum: http://bbs.espressif.com/
// example http://zeflo.com/2014/esp8266-weather-display/

/* Modified by Jonathan Valvano
 September 14, 2016
 Hardware connections
 Vcc is a separate regulated 3.3V supply with at least 215mA
 /------------------------------\
 |              chip      1   8 |
 | Ant                    2   7 |
 | enna       processor   3   6 |
 |                        4   5 |
 \------------------------------/
ESP8266    TM4C123
  1 URxD    PB1   UART out of TM4C123, 115200 baud
  2 GPIO0         +3.3V for normal operation (ground to flash)
  3 GPIO2         +3.3V
  4 GND     Gnd   GND (70mA)
  5 UTxD    PB0   UART out of ESP8266, 115200 baud
  6 Ch_PD         chip select, 10k resistor to 3.3V
  7 Reset   PB5   TM4C123 can issue output low to cause hardware reset
  8 Vcc           regulated 3.3V supply with at least 70mA
 */

/*
===========================================================
==========          CONSTANTS                    ==========
===========================================================
*/
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "Phone.h"
#include <ti/drivers/UART.h>
#include <ti/drivers/GPIO.h>
#include "Board.h"



// Access point parameters
#define SSID_NAME  "Samsung"
#define PASSKEY    "booosted"
//#define SEC_TYPE   ESP8266_ENCRYPT_MODE_WPA2_PSK

char ADC[] = "GET /query?city=Dallas,Texas&id=Mohammad%20Behnia&greet=T:181908.00%20N:DDMM.MMMMM%20W:DDDMM.MMMMM%20A:1234%20ID:9999%20Jumps:5&edxcode=8086 HTTP/1.1\r\nUser-Agent:Keil\r\nHost: ee445l-mb54229.appspot.com\r\n\r\n";

char tcp_head[] = "GET /query?city=Austin,Texas&id=Mohammad%20Behnia%20Ruibin%20Ni&greet=";
char tcp_tail[] = "&edxcode=8086 HTTP/1.1\r\nUser-Agent:Keil\r\nHost: ee445l-mb54229.appspot.com\r\n\r\n";

#define BUFFER_SIZE 1024
#define MAXTRY 1
// prototypes for functions defined in startup.s
#define MAX_NUM_RX_BYTES    2048   // Maximum RX bytes to receive in one go
#define MAX_NUM_TX_BYTES    1024   // Maximum TX bytes to send in one go

uint32_t wantedRxBytes;            // Number of bytes received so far
uint8_t RXBuffer[MAX_NUM_RX_BYTES];   // Receive buffer
uint8_t TXBuffer[MAX_NUM_TX_BYTES];   // Transmit buffer

UART_Handle uart7;
UART_Params uartParams7;
int rxBytes;
int readDone;

long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
// prototypes for helper functions
void DelayMs(uint32_t delay);
void ESP8266ProcessInput(const char* buffer);
void ESP8266HandleInputCommands(const char* input);
void ESP8266ParseSettingPointers(char* timePtr, char* shotsPtr, char* distPtr);
void ESP8266InitUART(void);
void ESP8266_PrintChar(char iput);
void ESP8266EnableRXInterrupt(void);
void ESP8266DisableRXInterrupt(void);
void ESP8266SendCommand(const char* inputString);
void ESP8266FIFOtoBuffer(void);


/*
=============================================================
==========            GLOBAL VARIABLES             ==========
=============================================================
*/

uint32_t RXBufferIndex = 0;
uint32_t LastRXIndex = 0;
uint32_t LastReturnIndex = 0;
uint32_t CurrentReturnIndex = 0;
//char RXBuffer[BUFFER_SIZE];
//char TXBuffer[BUFFER_SIZE];
#define SERVER_RESPONSE_SIZE 1024
char ServerResponseBuffer[SERVER_RESPONSE_SIZE]; // characters after +IPD,
uint32_t ServerResponseIndex = 0;

volatile bool ESP8266_ProcessBuffer = false;
volatile bool ESP8266_EchoResponse = false;
volatile bool ESP8266_ResponseComplete = false;
volatile bool ESP8266_APEnabled = false;
volatile bool ESP8266_ClientEnabled = false;
volatile bool ESP8266_ServerEnabled = false;
volatile bool ESP8266_InputProcessingEnabled = false;
volatile bool ESP8266_PageRequested = false;

/*
=======================================================================
==========              search FUNCTIONS                     ==========
=======================================================================
*/
char SearchString[32];
char FindString[32];
int FindIndex;
static bool FindFound;
volatile bool SearchLooking = false;
volatile bool SearchFound = false;
volatile uint32_t SearchIndex = 0;
char lc(char letter){
  if((letter>='A')&&(letter<='Z')) letter |= 0x20;
  return letter;
}

void Find(char *pt){
    FindFound = false;
    //strcpy(FindString,pt);
    FindIndex = 0;
    int i;
    char str[2] = {0, 0};
    for(i=0; i<rxBytes; RXBufferIndex++, i++) {
        str[0] = RXBuffer[RXBufferIndex];
        if((!FindFound) && (lc(str[0]) == pt[FindIndex])) {
            FindIndex++;
            if(pt[FindIndex] == 0){
                FindFound = true;
                FindIndex = 0;
            }
        }
        else {
            FindIndex = 0;
        }
        printf(str);
    }
}

//-------------------SearchStart -------------------
// - start looking for string in received data stream
// Inputs: none
// Outputs: none
void SearchStart(char *pt){
  strcpy(SearchString,pt);
  SearchIndex = 0;
  SearchFound = false;
  SearchLooking = true;
}
//-------------------SearchCheck -------------------
// - start looking for string in received data stream
// Inputs: none
// Outputs: none
void SearchCheck(char letter){
  if(SearchLooking){
    if(SearchString[SearchIndex] == lc(letter)){ // match letter?
      SearchIndex++;
      if(SearchString[SearchIndex] == 0){ // match string?
        SearchFound = true;
        SearchLooking = false;
      }
    }else{
      SearchIndex = 0; // start over
    }
  }
}

char ServerResponseSearchString[16]="+ipd,";
volatile uint32_t ServerResponseSearchFinished = false;
volatile uint32_t ServerResponseSearchIndex = 0;
volatile uint32_t ServerResponseSearchLooking = 0;

//-------------------ServerResponseSearchStart -------------------
// - start looking for server response string in received data stream
// Inputs: none
// Outputs: none
void ServerResponseSearchStart(void){
  strcpy(ServerResponseSearchString,"+ipd,");
  ServerResponseSearchIndex = 0;
  ServerResponseSearchLooking = 1; // means look for "+IPD"
  ServerResponseSearchFinished = 0;
  ServerResponseIndex = 0;
}

//-------------------ServerResponseSearchCheck -------------------
// - start looking for string in received data stream
// Inputs: none
// Outputs: none
void ServerResponseSearchCheck(char letter){
  if(ServerResponseSearchLooking==1){
    if(ServerResponseSearchString[ServerResponseSearchIndex] == lc(letter)){ // match letter?
      ServerResponseSearchIndex++;
      if(ServerResponseSearchString[ServerResponseSearchIndex] == 0){ // match string?
        ServerResponseSearchLooking = 2;
        strcpy(ServerResponseSearchString,"\r\nok\r\n");
        ServerResponseSearchIndex = 0;
      }
    }else{
      ServerResponseSearchIndex = 0; // start over
    }
  }else if(ServerResponseSearchLooking==2){
    if(ServerResponseIndex < SERVER_RESPONSE_SIZE){
      ServerResponseBuffer[ServerResponseIndex] = lc(letter); // copy data from "+IPD," to "OK"
      ServerResponseIndex++;
    }
    if(ServerResponseSearchString[ServerResponseSearchIndex] == lc(letter)){ // match letter?
      ServerResponseSearchIndex++;
      if(ServerResponseSearchString[ServerResponseSearchIndex] == 0){   // match OK string?
        ServerResponseSearchFinished = 1;
        ServerResponseSearchLooking = 0;
      }
    }else{
      ServerResponseSearchIndex = 0; // start over
    }
  }
}
/*
=======================================================================
==========         UART1 and private FUNCTIONS               ==========
=======================================================================
*/

// Read callback function
static void readCallback(UART_Handle handle, void *rxBuf, int size)
{
    //printf("read callback\n");
    rxBytes = size;
    readDone = 1;
}

// Write callback function
static void writeCallback(UART_Handle handle, void *rxBuf, int size)
{
    // Do nothing
}


//------------------- ESP8266_InitUART-------------------
// intializes uart and gpio needed to communicate with esp8266
// Configure UART1 for serial full duplex operation
// Inputs: baud rate (e.g., 115200 or 9600)
//         echo to UART0?
// Outputs: none
UART_Handle uart3;
UART_Params uartParams3;
void ESP8266_InitUART(uint32_t baud, int echo){
    volatile int delay;
    UART_init();
    // Specify non-default parameters
    UART_Params_init(&uartParams3);
    uartParams3.baudRate      = 115200;
    uartParams3.writeMode     = UART_MODE_BLOCKING;
    uartParams3.writeDataMode = UART_DATA_BINARY;
    uartParams3.writeCallback = writeCallback;
    uartParams3.writeTimeout = 50000;
    uartParams3.readMode      = UART_MODE_BLOCKING;
    uartParams3.readDataMode  = UART_DATA_BINARY;
    uartParams3.readCallback  = readCallback;
    uartParams3.readTimeout   = 50000;

    // Open the UART and initiate the first read
    uart3 = UART_open(Board_UART1, &uartParams3);
    if (uart3 == NULL) {
        printf("UART_open() failed!\n\r");
        while(1){};
    }
    ESP8266_EchoResponse = echo;
}

void ESP8266SendCommand(const char* inputString){
  int len = strlen(inputString);
  UART_write(uart7,inputString,len);
}
//--------ESP8266_PrintChar--------
// prints a character to the esp8226 via uart
// Inputs: character to transmit
// Outputs: none
// busy-wait synchronization
void ESP8266_PrintChar(char input){
    UART_write(uart3, &input, 1);
}

//----------ESP8266FIFOtoBuffer----------
// - copies uart fifo to RXBuffer, using a circular MACQ to store the last data
// - NOTE: would probably be better to use a software defined FIFO here
// - LastReturnIndex is index to previous \n
// Inputs: none
// Outputs:none
void ESP8266FIFOtoBuffer(void){
  char letter;
  int check = 1;
  while(check == 1){
    check = UART_read(uart3, &letter, 1);
    //letter = UART1_DR_R;        // retrieve char from FIFO
    if(ESP8266_EchoResponse){
      //UART_write(letter); // echo
    }
    if(RXBufferIndex >= BUFFER_SIZE){
      RXBufferIndex = 0; // store last BUFFER_SIZE received
    }
    RXBuffer[RXBufferIndex] = letter; // put char into buffer
    RXBufferIndex++; // increment buffer index
    SearchCheck(letter);               // check for end of command
    ServerResponseSearchCheck(letter); // check for server response
    if(letter == '\n'){
      LastReturnIndex = CurrentReturnIndex;
      CurrentReturnIndex = RXBufferIndex;
    }
  }
}

//---------ESP8266SendCommand-----
// - sends a string to the esp8266 module
// uses busy-wait
// however, hardware has 16 character hardware FIFO
// Inputs: string to send (null-terminated)
// Outputs: none


// DelayMs
//  - busy wait n milliseconds
// Input: time to wait in msec
// Outputs: none
void DelayMs(uint32_t n){
  volatile uint32_t time;
  while(n && !readDone){
    time = 4000;  // 1msec, tuned at 48 MHz
    while(time){
      time--;
    }
    n--;
  }
}
void DelayMsSearching(uint32_t n){
  volatile uint32_t time;
  while(n){
    time = 4095;  // 1msec, tuned at 80 MHz
    while(time){
      time--;
      if(SearchFound) return;
    }
    n--;
  }
}

/*
=======================================================================
==========          ESP8266 PUBLIC FUNCTIONS                 ==========
=======================================================================
*/
//-------------------ESP8266_Init --------------
// initializes the module as a client
// Inputs: baud rate: tested with 9600 and 115200
// Outputs: none

bool ESP8266_Init(void){
      /* GPIO Initialization */
      GPIO_init();
      GPIO_setConfig(Board_GPIO_RLED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
      GPIO_write(Board_GPIO_RLED, 1);

      /* UART Initialization */
      UART_Params_init(&uartParams7);
      uartParams7.baudRate      = 115200;
      uartParams7.writeMode     = UART_MODE_CALLBACK;
      uartParams7.writeDataMode = UART_DATA_BINARY;
      uartParams7.writeCallback = writeCallback;
      uartParams7.writeTimeout   = 10000000;
      uartParams7.readMode      = UART_MODE_CALLBACK;
      uartParams7.readDataMode  = UART_DATA_BINARY;
      uartParams7.readCallback  = readCallback;
      uartParams7.readTimeout   = 500000;
      uart7 = UART_open(Board_UART1, &uartParams7);
      if (uart7 == NULL) {
          printf("UART_open() failed!\n\r");
          //while(1){};
          return 0;
      }

      /* ESP8266 Initialization */
      ESP8266_Reset();

      printf("CCS WifiMode:\n");
      if(ESP8266_SetWifiMode(1)==0){
          printf("SetWifiMode error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS JoinAP:\n");
      if(ESP8266_JoinAccessPoint(SSID_NAME, PASSKEY)==0){
          printf("JoinAccessPoint error\n\r");
          return 0;
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS GetVersion:\n");
      if(ESP8266_GetVersionNumber()==0){
          printf("GetVersionNumber error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS GetIP:\n");
      if(ESP8266_GetIPAddress()==0){
          printf("GetIPAddress error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS GetStatus:\n");
      if(ESP8266_GetStatus()==1){
          printf("GetStatus error: no wifi\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS SetDataMode:\n");
      if(ESP8266_SetDataTransmissionMode(0)==0){
          printf("SetDataTransmissionMode error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      /*
      printf("CCS OpenTCP:\n");
      if(ESP8266_MakeTCPConnection("ee445l-mb54229.appspot.com")==0){
          printf("MakeTCPConnection error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS SendTCP:\n");
      if(ESP8266_SendTCP("message")==0){
          printf("SendTCP error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      printf("CCS CloseTCP:\n");
      if(ESP8266_CloseTCPConnection()==0){
          printf("CloseTCPConnection error\n\r");
          //while(1){};
      }
      printf("\n---\n");

      while(1){};*/


/*
    printf("ESP8266 Initialization:\n\r");
    // Step 1: AT+RST reset module
    if(ESP8266_Reset()==0){
        printf("Reset failure, could not reset\n\r");
//        while(1){};
    }
    //  UART_InChar();
//    ESP8266_InitUART(115200,true);

    // Step 2: AT+CWMODE=1 set wifi mode to client (not an access point)
    if(ESP8266_SetWifiMode(ESP8266_WIFI_MODE_CLIENT)==0){
        printf("SetWifiMode, could not set mode\n\r");
 //       while(1){};
    }

    // Step 3: AT+CWJAP="ssid","password"  connect to access point
    if(ESP8266_JoinAccessPoint(SSID_NAME,PASSKEY)==0){
        printf("JoinAccessPoint error, could not join AP\n\r");
//        while(1){};
    }

    // Optional Step: AT+CIFSR check to see our IP address
    if(ESP8266_GetIPAddress()==0){
        printf("GetIPAddress error, could not get IP address\n\r");
//        while(1){};
    }

    // Optional Step: AT+CIPMUX==0 set mode to single socket
    if(ESP8266_SetConnectionMux(0)==0){ // single socket
        printf("SetConnectionMux error, could not set connection mux\n\r"); while(1){};
    }

    // Optional Step: AT+CWLAP check to see other AP in area
    if(ESP8266_ListAccessPoints()==0){
        printf("ListAccessPoints, could not list access points\n\r");
//       while(1){};
    }

    // Step 4: AT+CIPMODE=0 set mode to not data mode
    if(ESP8266_SetDataTransmissionMode(0)==0){
        printf("SetDataTransmissionMode, could not make connection\n\r");
//       while(1){};
    }

    ESP8266_InputProcessingEnabled = false; // not a server
    while(1){};*/
}

bool ESP8266_TryConnect(void){
    printf("CCS JoinAP:\n");
    if(ESP8266_JoinAccessPoint(SSID_NAME, PASSKEY)==0){
        printf("JoinAccessPoint error\n\r");
        return 0;
        //while(1){};
    }
    printf("\n---\n");
    return 1;
}

char send_by_phone(char *message){
    printf("CCS GetStatus:\n");
    if(ESP8266_GetStatus()==1){
        printf("GetStatus error\n\r");
        //while(1){};
        return 0;
    }
    printf("\n---\n");

    printf("CCS OpenTCP:\n");
    if(ESP8266_MakeTCPConnection("ee445l-mb54229.appspot.com")==0){
        printf("MakeTCPConnection error\n\r");
        //while(1){};
        return 0;
    }
    printf("\n---\n");

    printf("CCS SendTCP:\n");
    if(ESP8266_SendTCP(message)==0){
        printf("SendTCP error\n\r");
        //while(1){};
    }
    printf("\n---\n");

    printf("CCS CloseTCP:\n");
    if(ESP8266_CloseTCPConnection()==0){
        printf("CloseTCPConnection error\n\r");
        //while(1){};
    }
    printf("\n---\n");
    return 1;
}

//----------ESP8266_Reset------------
// resets the esp8266 module
// input:  none
// output: 1 if success, 0 if fail
int ESP8266_Reset(){
/*  int try=10;
  SearchStart("ets");
  while(try){
    GPIO_write(Board_GPIO_RLED, 0);
    DelayMs(10);
    GPIO_write(Board_GPIO_RLED, 1);
    ESP8266SendCommand("AT+RST\r\n");
    DelayMs(500);
    Find("ets");
    if(FindFound) return 1; // success
    try-=1;
  }
  return 0; // fail*/
    int i;
    char str[2] = {0,0};
    printf("CCS Reset:\n");
    GPIO_write(Board_GPIO_RLED, 0);
    DelayMs(10);
    GPIO_write(Board_GPIO_RLED, 1);
//      UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+RST\r\n",strlen("AT+RST\r\n"));
    DelayMs(500);
    wantedRxBytes = 200;


//      UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+RST\r\n",strlen("AT+RST\r\n"));
    DelayMs(500);
    wantedRxBytes = 200;


    UART_read(uart7, &RXBuffer[RXBufferIndex], 500);
    UART_write(uart7,"AT+RST\r\n",strlen("AT+RST\r\n"));
    DelayMs(1000);
    wantedRxBytes = 200;

//      rxBytes = UART_read(uart7, &RXBuffer[RXBufferIndex], wantedRxBytes);
    for(i=0; i<rxBytes; RXBufferIndex++, i++)
    {
        str[0] = RXBuffer[RXBufferIndex];
        printf(str);
    }

    DelayMs(500);
    UART_readCancel(uart7);
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
//      UART_write(uart7,"AT+RST\r\n",strlen("AT+RST\r\n"));
    DelayMs(1000);
    UART_readCancel(uart7);
    for(i=0; i<rxBytes; RXBufferIndex++, i++)
    {
        str[0] = RXBuffer[RXBufferIndex];
        printf(str);
    }
    printf("\n---\n");
}

//---------ESP8266_SetWifiMode----------
// configures the esp8266 to operate as a wifi client, access point, or both
// since it searches for "no change" it will execute twice when changing modes
// Input: mode accepts ESP8266_WIFI_MODE constants
// output: 1 if success, 0 if fail
int ESP8266_SetWifiMode(uint8_t mode){
  //sprintf((char*)TXBuffer, "AT+CWMODE=%d\r\n", mode);
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CWMODE=1\r\n",strlen("AT+CWMODE=1\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_SetConnectionMux----------
// enables the esp8266 connection mux, required for starting tcp server
// Input: 0 (single) or 1 (multiple)
// output: 1 if success, 0 if fail
int ESP8266_SetConnectionMux(uint8_t enabled){
  int try=MAXTRY;
//  SearchStart("ok");
  while(try){
    sprintf((char*)TXBuffer, "AT+CIPMUX=%d\r\n", enabled);
    ESP8266SendCommand((const char*)TXBuffer);
    DelayMs(5000);
    Find("ok");
    if(FindFound) return 1; // success
    try--;
  }
  return 0; // fail
}

//----------ESP8266_JoinAccessPoint------------
// joins a wifi access point using specified ssid and password
// input:  SSID and PASSWORD
// output: 1 if success, 0 if fail
int ESP8266_JoinAccessPoint(const char* ssid, const char* password){
  //sprintf((char*)TXBuffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CWJAP=\"Samsung\",\"booosted\"\r\n",strlen("AT+CWJAP=\"Samsung\",\"booosted\"\r\n"));
    DelayMs(7000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_ListAccessPoints----------
// lists available wifi access points
// Input: none
// output: 1 if success, 0 if fail
int ESP8266_ListAccessPoints(void){
  int try=MAXTRY;
//  SearchStart("ok");
  while(try){
    ESP8266SendCommand("AT+CWLAP\r\n");
    DelayMs(8000);
    Find("ok");
    if(FindFound) return 1; // success
    try--;
  }
  return 0; // fail
}

// ----------ESP8266_QuitAccessPoint-------------
// - disconnects from currently connected wifi access point
// Inputs: none
// Outputs: 1 if success, 0 if fail
int ESP8266_QuitAccessPoint(void){
  int try=MAXTRY;
//  SearchStart("ok");
  while(try){
    ESP8266SendCommand("AT+CWQAP\r\n");
    DelayMs(8000);
    Find("ok");
    if(FindFound) return 1; // success
    try--;
  }
  return 0; // fail
}

//----------ESP8266_ConfigureAccessPoint------------
// configures esp8266 wifi access point settings
// use this function only when in AP mode (and not in client mode)
// input:  SSID, Password, channel, security
// output: 1 if success, 0 if fail
int ESP8266_ConfigureAccessPoint(const char* ssid, const char* password, uint8_t channel, uint8_t encryptMode){
  int try=MAXTRY;
//  SearchStart("ok");
  while(try){
    sprintf((char*)TXBuffer, "AT+CWSAP=\"%s\",\"%s\",%d,%d\r\n", ssid, password, channel, encryptMode);
    ESP8266SendCommand((const char*)TXBuffer);
    DelayMs(4000);
    Find("ok");
    if(FindFound) return 1; // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_GetIPAddress----------
// Get local IP address
// Input: none
// output: 1 if success, 0 if fail
int ESP8266_GetIPAddress(void){
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CIFSR\r\n",strlen("AT+CIFSR\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_MakeTCPConnection----------
// Establish TCP connection
// Input: IP address or web page as a string
// output: 1 if success, 0 if fail
int ESP8266_MakeTCPConnection(char *IPaddress){
  //sprintf((char*)TXBuffer, "AT+CIPSTART=\"TCP\",\"%s\",80\r\n", IPaddress);
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CIPSTART=\"TCP\",\"172.217.22.116\",80\r\n",strlen("AT+CIPSTART=\"TCP\",\"172.217.22.116\",80\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_SendTCP----------
// Send a TCP packet to server
// Input: TCP payload to send
// output: 1 if success, 0 if fail
int ESP8266_SendTCP(char* fetch){
  //sprintf((char*)TXBuffer, "AT+CIPSEND=%d\r\n", strlen(fetch));
  int k;
  for(k = 0; k<=70; k++){
      ADC[k+56] = fetch[k];
  }
  int sent1, sent2;
  int packetsize = strlen(ADC);
  int try=1;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 500);
    sent1 = UART_write(uart7,"AT+CIPSEND=204\r\n",strlen("AT+CIPSEND=204\r\n"));
    DelayMs(50);
    sent2 = UART_write(uart7,&ADC[0],packetsize);
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("+ipd");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_CloseTCPConnection----------
// Close TCP connection
// Input: none
// output: 1 if success, 0 if fail
int ESP8266_CloseTCPConnection(void){
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CIPCLOSE\r\n",strlen("AT+CIPCLOSE\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}
//---------ESP8266_SetDataTransmissionMode----------
// set data transmission mode
// Input: 0 not data mode, 1 data mode; return "Link is builded"
// output: 1 if success, 0 if fail
int ESP8266_SetDataTransmissionMode(uint8_t mode){
  //sprintf((char*)TXBuffer, "AT+CIPMODE=%d\r\n", mode);
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CIPMODE=0\r\n",strlen("AT+CIPMODE=0\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_GetStatus----------
// get status
// Input: none
// output: 1 if success, 0 if fail
int ESP8266_GetStatus(void){
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+CIPSTATUS\r\n",strlen("AT+CIPSTATUS\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("status:5");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}

//---------ESP8266_GetVersionNumber----------
// get status
// Input: none
// output: 1 if success, 0 if fail
int ESP8266_GetVersionNumber(void){
  int try=MAXTRY;
  SearchStart("ok");
  while(try){
    rxBytes = 0;
    readDone = 0;
    UART_read(uart7, &RXBuffer[RXBufferIndex], 200);
    UART_write(uart7,"AT+GMR\r\n",strlen("AT+GMR\r\n"));
    DelayMs(2000);
    UART_readCancel(uart7);
    Find("ok");
    if(FindFound) {return 1;} // success
    try--;
  }
  return 0; // fail
}
