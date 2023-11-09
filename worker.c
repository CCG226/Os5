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


	TaskHandler();//do worker task


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
void TaskHandler()
{

	int msqid = AccessMsgQueue();

	struct Sys_Time* Clock = AccessSystemTime();

	//determines of worker can keep working, if 0 worker must terminate
	int status = RUNNING;

	msgbuffer msg;

	//while status is NOT 0, keep working
	while(status != TERMINATING)
	{ 

	}

	DisposeAccessToShm(Clock);

}


double GenerateEventWaitTime()
{
	//determine random amount of time [0,5] seconds + [0,1000] milliseconds workrer must wait in blocked queue before accessing external resource
	int eventWaitSeconds = (rand() % 6);

	int eventWaitMilliseconds = (rand() % 1001);

	double convertMilliToSecs = (double) eventWaitMilliseconds / 1000;

	return (eventWaitSeconds + convertMilliToSecs);

}
int SendResourceRequest(int msqid, msgbuffer *msg)
{
	//wait for os to send message with amount of time we can run for

	if(msgrcv(msqid, msg, sizeof(msgbuffer), getpid(), 0) == -1)
	{
		printf("Failed To Get Message Request From Os. Worker: %d\n", getpid());
		fprintf(stderr, "errno: %d\n", errno);
		exit(1);
	}
	//return timeslice
	return msg->timeslice;
}
void GetResourceResponse(int msqid, msgbuffer *msg, int timeRan)
{//send amount of time worker ran and  amount of time worker wait to access external resource (if it doesnt, eventWaitTime = 0)
	msg->timeslice = timeRan;
	msg->mtype = 1;

	//send message back to os
	if(msgsnd(msqid, msg, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
		perror("Failed To Generate Response Message Back To Os.\n");
		exit(1);
	}

}
