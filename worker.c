#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "worker.h"
#include <errno.h>
#include <time.h>
//Author: Connor Gilmore
//Purpose: takes in a numeric argument
//Program will take in two arguments that represent a random time in seconds and nanoseconds
//are program will then work for that amoount of time by printing status messages
//are worker will constantly check the os's system clock to see if it has reached the end of its work time, if it has it will terminate

int main(int argc, char** argv)
{
	srand(getpid());

	int workerResources[RES_AMOUNT];

	for(int i = 0; i < RES_AMOUNT;i++)
	{
	workerResources[i] = 0;
	}


	TaskHandler(workerResources);//do worker task


	return EXIT_SUCCESS;
}
int AccessMsgQueue()
{
	//access message queue create by workinh using key constant
	int msqid = -1;
	if((msqid = msgget(MSG_SYSTEM_KEY, 0777)) == -1)
	{
		perror("Failed To Join Os's Message Queue.\n");
		exit(1);
	}
	return msqid;
}
struct Sys_Time* AccessSystemTime()
{//access system clock from shared memory (read-only(
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0444);
	if(shm_id == -1)
	{
		perror("Failed To Access System Clock");

		exit(1);	
	}
	return (struct Sys_Time*)shmat(shm_id, NULL, 0);

}
void DisposeAccessToShm(struct Sys_Time* clock)
{//detach system clock from shared memory
	if(shmdt(clock) == -1)
	{
		perror("Failed To Release Shared Memory Resources.\n");
		exit(1);
	}

}

//workers console print task
void TaskHandler(int workerResources[])
{

	int msqid = AccessMsgQueue();

	struct Sys_Time* Clock = AccessSystemTime();

	//determines of worker can keep working, if 0 worker must terminate
	int status = RUNNING;
	msgbuffer msg;

	int qtrSec = 0;
	int qtrNano = 0;

	int requestEventSec = 0;
	int requestEventNano = 0;

	int timeUntilNxtRequest = GenerateRequestTime();

	GenerateTimeToEvent(Clock->seconds, Clock->nanoseconds, timeUntilNxtRequest,0, &requestEventSec, &requestEventNano);

	GenerateTimeToEvent(Clock->seconds, Clock->nanoseconds, MAX_NANOSECOND / 4,0,&qtrSec, &qtrNano);

	//while status is NOT 0, keep working
	while(status != TERMINATING)
	{
	if(CanEvent(Clock->seconds, Clock->nanoseconds, requestEventSec, requestEventNano) == 1)
	{
		
		int choice = ClaimOrReleaseResource();
		int resourceId = 0;
		
		if(choice == 0)
		{
		//claim
		resourceId = ClaimResource();
		}
		else{
		//release
		resourceId = ReleaseResource(workerResources);
		if(resourceId == -1)
		{
		resourceId = ClaimResource();
		}

		}

		SendRequest(msqid,&msg, resourceId, choice);

		if(choice == 0)
		{
		
		int newResource = GetResourceResponse(msqid, &msg);

		AppendResource(workerResources, newResource);

		}

		timeUntilNxtRequest = GenerateRequestTime();	

		GenerateTimeToEvent(Clock->seconds, Clock->nanoseconds, timeUntilNxtRequest,0, &requestEventSec, &requestEventNano);

	}	

         if(CanEvent(Clock->seconds, Clock->nanoseconds, qtrSec, qtrNano) == 1)
	 {
	   GenerateTimeToEvent(Clock->seconds, Clock->nanoseconds,  MAX_NANOSECOND / 4,0,&qtrSec, &qtrNano);
   		
	   if(ShouldTerminate() == 1)
	   {
	  printf("worker %d is termingating\n", getpid());	   
	     status = TERMINATING;	   
	   }	   
	 }	 
	}

	DisposeAccessToShm(Clock);

}
int ClaimResource()
{
return (rand() % 10); 
}
int ReleaseResource(int resourceArray[])
{
for(int i = 0; i < RES_AMOUNT; i++)
{
if(resourceArray[i] > 0)
{
resourceArray[i]--;

return i;
}

}
return -1;
}
void AppendResource(int resourceArray[], int id)
{
for(int i = 0; i < RES_AMOUNT; i++)
{
if(i == id)
{
resourceArray[i]++;
}
}
}
int GenerateRequestTime()
{
 return (rand() % REQUEST_BOUND);
}

void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano)
{//adds timeInterval time to current time (which is system clock) and stores result in event time ptr variables to be used to know when a particular event will occur on the system clock
	*(eventSec) = currentSecond + timeIntervalSec;
	if(currentNano + timeIntervalNano >= MAX_NANOSECOND)
	{
		*(eventNano) = (currentNano + timeIntervalNano) - MAX_NANOSECOND;
		*(eventSec) = *(eventSec) + 1;
	}
	else
	{
		*(eventNano) = (currentNano + timeIntervalNano);
	}
}
int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark)
{
if(eventSecMark <= curSec && eventNanoMark <= curNano)
{
return 1;
}

return 0;
}
int ShouldTerminate()
{
 int option = (rand() % 100) + 1;

 if(option <= 3)
	{
	return 1;
	}
 return 0;
}

int GetResourceResponse(int msqid, msgbuffer *msg)
{

	//wait for os to send message with amount of time we can run for

	if(msgrcv(msqid, msg, sizeof(msgbuffer), getpid(), 0) == -1)
	{
		printf("Failed To Get Message Request From Os. Worker: %d\n", getpid());
		fprintf(stderr, "errno: %d\n", errno);
		exit(1);
	}
	//return timeslice
	return msg->resourceID;
}
void SendRequest(int msqid, msgbuffer *msg,int resourceId, int requestAction)
{//send amount of time worker ran and  amount of time worker wait to access external resource (if it doesnt, eventWaitTime = 0)
	msg->resourceID = resourceId;
	msg->action = requestAction;
	msg->workerID = getpid();
	//send message back to os
	if(msgsnd(msqid, msg, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
		perror("Failed To Generate Response Message Back To Os.\n");
		exit(1);
	}

}

int ClaimOrReleaseResource()
{
int option = (rand() % 100) + 1;

if(option <= 25)
{
return 1;
}
return 0;
}
