#include "Messages.h"
#include "broadcast.h"

#define THREADSTACKSIZE    1024
#define MESSAGE_TASK_STACK_SIZE 1024
#define MESSAGE_TASK_PRIORITY   5
static void * sendData;

extern char inBuff[MESSAGE_SIZE];
extern char outBuff[MESSAGE_SIZE];

static Task_Params messageTaskParams;
Task_Struct messageTask;    /* not static so you can see in ROV */
static uint8_t messageTaskStack[MESSAGE_TASK_STACK_SIZE];
static Semaphore_Handle messageSem;
static void messageLoop(UArg arg0, UArg arg1)
{
    /*
    outBuff = (MESSAGE *) malloc(MESSAGE_SIZE);
    inBuff = (MESSAGE *) malloc(MESSAGE_SIZE);
    outBuff->message = outBuff+sizeof(MESSAGE);
    inBuff->message = outBuff+sizeof(MESSAGE);


    inBuff = (MESSAGE *)inStr;
    outBuff = (MESSAGE *)outStr;
     */

    //sprintf((char *)outBuff,"INCOMMING MESSAGE\n");
    printf("Messages.c outBuff: %d\n",outBuff);

    Semaphore_Params params;
    Error_Block eb;
    Semaphore_Params_init(&params);
    Error_init(&eb);
    messageSem = Semaphore_create(0, &params, &eb);
    if(messageSem == NULL)
    {
        printf("Semaphore creation failed!\n");
        System_abort("messageSem creation failed");
    }
    //printf("Testing Buffer test buffer\n");
    //printf("Test Buffer: %s\n",outStr);
    while(1){

        Semaphore_pend(messageSem,BIOS_WAIT_FOREVER);

        //printf("Messages.c sendDataBuff: %d\n",sendData);

        sendMessageH(sendData);
        //printf("Done Send\n");
    }
}
void sendMessage()
{
    sendData = outBuff;
    Semaphore_post(messageSem);
}
void forwardMessage()
{
    sendData = inBuff;
    Semaphore_post(messageSem);
}
void init_messages()
{



    Task_Params_init(&messageTaskParams);

    messageTaskParams.stackSize = MESSAGE_TASK_STACK_SIZE;
    messageTaskParams.priority = MESSAGE_TASK_PRIORITY;
    messageTaskParams.stack = &messageTaskStack;
    messageTaskParams.arg0 = (UInt)1000000;

    Task_construct(&messageTask, messageLoop, &messageTaskParams, NULL);
}

