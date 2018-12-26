#ifndef BROADCAST_H
#define BROADCAST_H

#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/PIN.h>
#include "Board.h"
#include "easylink/EasyLink.h"
#include <stdio.h>


void receivePayload();
//void sendMessageH(char * data, uint8_t data_size);
void sendMessageH(void * data);
void broadcast_init();
void startReceiver();
void takeRadio();
void giveRadio();
#endif
