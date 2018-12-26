

#include "broadcast.h"
#include "Messages.h"
#define SEN 1
#define REC 0
#define JOB REC


//TX
#define RFEASYLINKTX_TASK_STACK_SIZE    1024
#define RFEASYLINKTX_TASK_PRIORITY      2
#define RFEASYLINKTX_BURST_SIZE         10
#define RFEASYLINKTXPAYLOAD_LENGTH      30
Task_Struct txTask;    /* not static so you can see in ROV */
static Task_Params txTaskParams;
static uint8_t txTaskStack[RFEASYLINKTX_TASK_STACK_SIZE];


#define RFEASYLINKRX_ADDR_FILTER
#define RFEASYLINKEX_TASK_STACK_SIZE 1024
#define RFEASYLINKEX_TASK_PRIORITY   2

/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;


PIN_Config pinTable[] = {
    Board_PIN_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_PIN_LED2 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
#if defined __CC1352R1_LAUNCHXL_BOARD_H__
    Board_DIO30_RFSW | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
#endif
    PIN_TERMINATE
};


static Task_Params rxTaskParams;
Task_Struct rxTask;    /* not static so you can see in ROV */
static uint8_t rxTaskStack[RFEASYLINKEX_TASK_STACK_SIZE];


PIN_Handle pinHandle;

static Semaphore_Handle radioSem;
static Semaphore_Handle rxSem;
static Semaphore_Handle txSem;
//static char radioPend;


extern char inBuff[MESSAGE_SIZE];
extern char outBuff[MESSAGE_SIZE];

void txDoneCb(EasyLink_Status status)
{
    if(status == EasyLink_Status_Success)
    {
        printf("Message Sent!\n");
    }
    if(status == EasyLink_Status_Aborted)
    {
        printf("Send Aborted!\n");
    }
    Semaphore_post(txSem);
    Semaphore_post(radioSem);


}
void rxDoneCb(EasyLink_RxPacket * rxPacket, EasyLink_Status status)
{
    MESSAGE * inMsg = (MESSAGE *) inBuff;
    Semaphore_post(rxSem);
    if(status == EasyLink_Status_Success)
    {
        memcpy(inBuff,rxPacket->payload,MESSAGE_SIZE);
        inMsg->message = inBuff + sizeof(MESSAGE);
        receivePayload();
    }
    if(status == EasyLink_Status_Aborted)
    {
        printf("Receive Aborted!\n");
    }
    Semaphore_post(radioSem);


}



void receive()
{
    UART_init();
    GPS_Init();
    ESP8266_Init();
    while(1) {
         printf("Receiver Pending\n");
         Semaphore_pend(radioSem, BIOS_WAIT_FOREVER);
         EasyLink_receiveAsync(rxDoneCb, 0);
         if(Semaphore_pend(rxSem, (100000000 / Clock_tickPeriod)) == FALSE)
         {
             if(EasyLink_abort() == EasyLink_Status_Success)
             {
                Semaphore_pend(rxSem, BIOS_WAIT_FOREVER);
             }

         }

     }
}
void sendMessageH(void * data)
{
        EasyLink_abort();
        if(Semaphore_pend(radioSem, (1000000 / Clock_tickPeriod)) == FALSE)
        {
            return;
        }
        uint32_t absTime;
        uint8_t txBurstSize = 0;

        EasyLink_TxPacket txPacket =  { {0}, 0, 0, {0} };

        memcpy(txPacket.payload,data,MESSAGE_SIZE);


        txPacket.len = MESSAGE_SIZE;
        if(MESSAGE_SIZE<EASYLINK_MAX_DATA_LENGTH)
            txPacket.len = MESSAGE_SIZE;
        else
            txPacket.len = EASYLINK_MAX_DATA_LENGTH;
        txPacket.dstAddr[0] = 0xaa;

        /* Add a Tx delay for > 500ms, so that the abort kicks in and brakes the burst */
        if(EasyLink_getAbsTime(&absTime) != EasyLink_Status_Success)
        {
            // Problem getting absolute time
        }
        if(txBurstSize++ >= RFEASYLINKTX_BURST_SIZE)
        {
          /* Set Tx absolute time to current time + 1s */
          txPacket.absTime = absTime + EasyLink_ms_To_RadioTime(1000);
          txBurstSize = 0;
        }
        /* Else set the next packet in burst to Tx in 100ms */
        else
        {
          /* Set Tx absolute time to current time + 100ms */
          txPacket.absTime = absTime + EasyLink_ms_To_RadioTime(100);
        }


        EasyLink_transmitAsync(&txPacket, txDoneCb);

        if(Semaphore_pend(txSem, (6000000 / Clock_tickPeriod)) == FALSE)
        {
            if(EasyLink_abort() == EasyLink_Status_Success)
            {
               Semaphore_pend(txSem, BIOS_WAIT_FOREVER);
            }
        }

}

static void rfEasyLinkTxFnx(UArg arg0, UArg arg1)
{
        EasyLink_Params easyLink_params;
        EasyLink_Params_init(&easyLink_params);

        easyLink_params.ui32ModType = EasyLink_Phy_Custom;

        /* Initialize EasyLink */
        if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
        {
            System_abort("EasyLink_init failed");
        }

        /*
         * If you wish to use a frequency other than the default, use
         * the following API:
         * EasyLink_setFrequency(868000000);
         */

    #if (defined __CC1352P1_LAUNCHXL_BOARD_H__)
        /* Set output power to 20dBm */
        EasyLink_Status pwrStatus = EasyLink_setRfPower(20);
    #else
        /* Set output power to 12dBm */
        EasyLink_Status pwrStatus = EasyLink_setRfPower(12);
    #endif
        if(pwrStatus != EasyLink_Status_Success)
        {
            // There was a problem setting the transmission power
            while(1);
        }
}
static void rfEasyLinkRxFnx(UArg arg0, UArg arg1)
{
    Semaphore_Params params;
    Error_Block eb;
    Semaphore_Params_init(&params);
    Error_init(&eb);
    radioSem = Semaphore_create(1, &params, &eb);

    if(radioSem == NULL)
    {
        printf("Semaphore creation failed!\n");
        System_abort("Semaphore creation failed");
    }
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);
    easyLink_params.ui32ModType = EasyLink_Phy_Custom;
    if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_abort("EasyLink_init failed");
    }

}


void rxSemCreate()
{
    Semaphore_Params params;
    Error_Block eb;
    Semaphore_Params_init(&params);
    Error_init(&eb);
    rxSem = Semaphore_create(0, &params, &eb);
    if(rxSem == NULL)
    {
        printf("Semaphore creation failed!\n");
        System_abort("rxSem creation failed");
    }

}

void txSemCreate()
{

    Semaphore_Params params;
    Error_Block eb;
    Semaphore_Params_init(&params);
    Error_init(&eb);
    txSem = Semaphore_create(0, &params, &eb);
    if(txSem == NULL)
    {
        printf("Semaphore creation failed!\n");
        System_abort("txSem creation failed");
    }

}
static void rfEasyLinkFnx(UArg arg0, UArg arg1)
{
    rxSemCreate();
    txSemCreate();
    rfEasyLinkTxFnx(arg0,arg1);
    rfEasyLinkRxFnx(arg0,arg1);
    receive();
}

void broadcast_init()
{
     /* Call driver init functions */
    //Board_initGeneral();



    /* Open LED pins */
    ledPinHandle = PIN_open(&ledPinState, pinTable);
    Assert_isTrue(ledPinHandle != NULL, NULL);

    /* Clear LED pins */
    PIN_setOutputValue(ledPinHandle, Board_PIN_LED1, 0);
    PIN_setOutputValue(ledPinHandle, Board_PIN_LED2, 0);

    pinHandle = ledPinHandle;

    Task_Params_init(&rxTaskParams);
    rxTaskParams.stackSize = RFEASYLINKEX_TASK_STACK_SIZE;
    rxTaskParams.priority = RFEASYLINKEX_TASK_PRIORITY;
    rxTaskParams.stack = &rxTaskStack;
    rxTaskParams.arg0 = (UInt)1000000;

    Task_construct(&rxTask, rfEasyLinkFnx, &rxTaskParams, NULL);

    /* Start BIOS */

}
void startReceiver()
{
    BIOS_start();
}

