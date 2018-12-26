#ifndef MSG_H
#define MSG_H
#include "Constants.h"

typedef struct mess_strct
{
	int id;
	char jump;
	char * message;
} MESSAGE;

//MESSAGE * inBuff;
//MESSAGE * outBuff;

void init_messages(); //Not Needed
void sendMessage();
void forwardMessage();

#endif
