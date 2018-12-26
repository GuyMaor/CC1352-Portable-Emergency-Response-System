#include "broadcast.h"
#include "Button.h"
#include "ID_List.h"
#include "Phone.h"
#include "Messages.h"
#include "GPS.h"
#include <ti/drivers/UART.h>
#include <ti/drivers/GPIO.h>

char inBuff[MESSAGE_SIZE];
char outBuff[MESSAGE_SIZE];

void receivePayload()
{
    MESSAGE * inMsg = (MESSAGE *) inBuff;
    printf("RECEIVED PAYLOAD: %d %d %s\n",inMsg->id,inMsg->jump,inMsg->message);
        if(in_list(inMsg->id))
    {
        printf("In List\n");
        return;
    }
    if(!add_id(inMsg->id))
        return;
    //First attempt to send by phone.
    if(send_by_phone(inMsg->message))
        return;
    else //Attempt to broadcast.
    {
        if(inMsg->jump) //Drop if max jumps have reached.
        {
            printf("FORWARDING\n");
            inMsg->jump--;
            forwardMessage();
        }

    }
}
int main(void)
{

    //sprintf(outStr,"TESTING BUFFER\n");
    /* Call driver init functions */
    init_messages();

    init_id_list();
    Board_initGeneral();
    Button_init();
    broadcast_init();
//    startReceiver();
    BIOS_start();
    return (0);
}
