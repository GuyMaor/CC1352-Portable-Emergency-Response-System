#include "ID_List.h"
#include <time.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/knl/Clock.h>
#define THREADSTACKSIZE    1024
#define CLEAR_IDS_TASK_STACK_SIZE 1024
#define CLEAR_IDS_TASK_PRIORITY   5
static Task_Params clearIDsTaskParams;
Task_Struct clearIDsTask;
static uint8_t clearIDsTaskStack[CLEAR_IDS_TASK_STACK_SIZE];
static Semaphore_Handle clearIDsSem;

int id_list [MAX_IDS];


static void clearIDsLoop(UArg arg0, UArg arg1)
{
    Semaphore_Params params;
    Error_Block eb;
    Semaphore_Params_init(&params);
    Error_init(&eb);
    clearIDsSem = Semaphore_create(0, &params, &eb);
    if(clearIDsSem == NULL)
    {
        printf("clearIDsSem creation failed!\n");
    }
    while(1)
    {
        if(Semaphore_pend(clearIDsSem, (20000000 / Clock_tickPeriod)) == FALSE)
        {
            printf("Clearing IDs\n");
            clear_ids();
        }
        else
            printf("Reset ID Clear Timer\n");
    }
}

void init_id_list()
{
	clear_ids();
	srand(time(NULL));

    Task_Params_init(&clearIDsTaskParams);
    clearIDsTaskParams.stackSize = CLEAR_IDS_TASK_STACK_SIZE;
    clearIDsTaskParams.priority = CLEAR_IDS_TASK_PRIORITY;
    clearIDsTaskParams.stack = &clearIDsTaskStack;
    clearIDsTaskParams.arg0 = (UInt)1000000;
    Task_construct(&clearIDsTask, clearIDsLoop, &clearIDsTaskParams, NULL);
}

char add_id(int id)
{
	int hash = id%MAX_IDS;
	int i;
	for(i = 0; i<MAX_IDS; i++)
	{
		if(!id_list[hash])
		{
			id_list[hash] = id;
			Semaphore_post(clearIDsSem);
			return 1;
		}
		hash++;
		hash%=MAX_IDS;
	}
	return 0;
}
void clear_ids() //NEEDS TO BE USED
{
	int i;
	for(i = 0; i<MAX_IDS; i++)
	{
		id_list[i] = 0;
	}
}

char in_list(int id)
{
	int  hash = id%MAX_IDS;
	int i;
	for(i = 0; i<MAX_IDS; i++)
	{
		if(id_list[hash] == id)
			return 1;
		if(id_list[hash] == 0)
			return 0;
		hash++;
		hash%=MAX_IDS;
	}
	return 0;
}
int createID()
{
    return rand();
}

