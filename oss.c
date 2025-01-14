#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include "oss.h"
#include <string.h>
#include "stdarg.h"
//Author: Connor Gilmore
//Purpose: Program task is to scheudle, handle and launch x amount of worker processes 
//The Os will terminate no matter what after 3 seconds
//the OS operates its own system clock it stores in shared memory to control worker runtimes
//the OS stores worker status information in a process table
//program takes arguments: 
//-h for help,
//-n for number of child processes to run
//-s for number of child processes to run simultanously
//-t time interval to launch nxt child
//-f for the name of the  log file you want os to log too. (reccommend .txt)
void Begin_OS_LifeCycle()
{
	printf("\n\nBooting Up Operating System...\n\n");	
	//set alarm to trigger a method to kill are program after 3 seconds via signal
	signal(SIGALRM, End_OS_LifeCycle);

	alarm(10);

}

void End_OS_LifeCycle()
{
	//clear shared memmory and message queue as the program failed to reack EOF
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777);
	shmctl(shm_id, IPC_RMID, NULL);
	int msqid = msgget(MSG_SYSTEM_KEY, 0777);
	msgctl(msqid, IPC_RMID, NULL);
	//kill system
	printf("\n\nOS Has Reached The End Of Its Life Cycle And Has Terminated.\n\n");	
	exit(1);

}	

int main(int argc, char** argv)
{
	srand(time(NULL));

	//set 3 second timer	
	Begin_OS_LifeCycle();
	//avialbality vector
	int resourceDescriptor[RES_AMOUNT];

	BuildResourceDescriptor(resourceDescriptor);

	//allocation table
	int allocationTable[TABLE_SIZE][RES_AMOUNT];

	BuildAllocationTable(allocationTable);

	//process table
	struct PCB processTable[TABLE_SIZE];

	//assigns default values to processtable elements (constructor)
	BuildProcessTable(processTable);

	//stores shared memory system clock info (seconds passed, nanoseconds passed, clock speed(rate))
	struct Sys_Time *OS_Clock;

	//default values for argument variables are 0
	int workerAmount = 0;//-n proc value (number of workers to launch)
	int simultaneousLimit = 0;//-s simul value (number of workers to run in sync)
	int timeInterval = 0;//-t iter value (time interval to launch nxt worker every t time)
	char* logFileName = NULL;//-f value to store file name of log file


	//stored id to access shared memory for system clock
	int shm_ClockId = -1;

	//pass workerAmount (-n), simultaneousLimit (-s),timeLimit(-t) by reference and assigns coresponding argument values from the command line to them
	ArgumentParser(argc, argv,&workerAmount, &simultaneousLimit, &timeInterval, &logFileName);

	//'starts' aka sets up shared memory system clock and assigns default values for clock 
	shm_ClockId = StartSystemClock(&OS_Clock);

	//handles how os laumches, tracks, and times workers
	WorkerHandler(workerAmount, simultaneousLimit, timeInterval,logFileName, OS_Clock, processTable,resourceDescriptor, allocationTable );

	//removes system clock from shared mem
	StopSystemClock(OS_Clock ,shm_ClockId);
	printf("\n\n\n");
	return EXIT_SUCCESS;

}
int ConstructMsgQueue()
{
	int id = -1;
	//create a message queue and return id of queue
	if((id = msgget(MSG_SYSTEM_KEY, 0777 | IPC_CREAT)) == -1)
	{
		printf("Failed To Construct OS's Msg Queue\n");
		exit(1);

	}
	return id;
}
void DestructMsgQueue(int msqid)
{
	//delete message queue from system
	if(msgctl(msqid, IPC_RMID, NULL) == -1)
	{
		printf("Failed To Destroy Message Queue\n");
		exit(1);
	}
}
int RequestHandler(int msqid, msgbuffer *msg) 
{
	//see if workers sent a message
	ssize_t result = msgrcv(msqid, msg, sizeof(msgbuffer), 1, IPC_NOWAIT);
	if(result != -1)
	{
		return 1;
	}
	return 0;
}
void ResponseHandler(int msqid, int workerId, msgbuffer *msg)
{//send response back to woker saying we gave them thier claim or accepted their release of a resource
	msg->mtype = msg->workerID;	
	if(msgsnd(msqid,msg, sizeof(msgbuffer)-sizeof(long),0) == -1)
	{
		printf("Worker terminated without resource response\n");
		exit(1);
	}

}
int StartSystemClock(struct Sys_Time **Clock)
{
	//sets up shared memory location for system clock
	int shm_id = shmget(SYS_TIME_SHARED_MEMORY_KEY, sizeof(struct Sys_Time), 0777 | IPC_CREAT);
	if(shm_id == -1)
	{
		printf("Failed To Create Shared Memory Location For OS Clock.\n");

		exit(1);
	}
	//attaches system clock to shared memory
	*Clock = (struct Sys_Time *)shmat(shm_id, NULL, 0);
	if(*Clock == (struct Sys_Time *)(-1)) {
		printf("Failed to connect OS Clock To A Slot In Shared Memory\n");
		exit(1);
	}
	//set default clock values
	//clock speed is 50000 nanoseconds per 'tick'
	(*Clock)->rate = 0;
	(*Clock)->seconds = 0;
	(*Clock)->nanoseconds = 0;

	return shm_id;
}
void StopSystemClock(struct Sys_Time *Clock, int sharedMemoryId)
{ //detach shared memory segement from our system clock
	if(shmdt(Clock) == -1)
	{
		printf("Failed To Release Shared Memory Resources..");
		exit(1);
	}
	//remove shared memory location
	if(shmctl(sharedMemoryId, IPC_RMID, NULL) == -1) {
		printf("Error Occurred When Deleting The OS Shared Memory Segmant");
		exit(1);
	}
}
void RunSystemClock(struct Sys_Time *Clock, int incRate) {

	if(incRate < 0)
	{
		incRate = incRate * -1;
	}
	//clock iteration based on incRate value, if negative (worker terminating time slice return) change value to positive
	Clock->rate = incRate;
	//handles clock iteration / ticking
	if((Clock->nanoseconds + Clock->rate) >= MAX_NANOSECOND)
	{
		Clock->nanoseconds = (Clock->nanoseconds + Clock->rate) - MAX_NANOSECOND;	
		Clock->seconds = Clock->seconds + 1;
	}
	else {
		Clock->nanoseconds = Clock->nanoseconds + Clock->rate;
	}


}


void ArgumentParser(int argc, char** argv, int* workerAmount, int* workerSimLimit, int* timeInterval, char** fileName) {
	//assigns argument values to workerAmount, simultaneousLimit, workerArg variables in main using ptrs 
	int option;
	//getopt to iterate over CL options
	while((option = getopt(argc, argv, "hn:s:t:f:")) != -1)
	{
		switch(option)
		{//junk values (non-numerics) will default to 0 with atoi which is ok
			case 'h'://if -h
				Help();
				break;
			case 'n'://if -n int_value
				*(workerAmount) = atoi(optarg);
				break;
			case 's'://if -s int_value
				*(workerSimLimit) = atoi(optarg);
				break;
			case 't'://if t int_value
				*(timeInterval) = atoi(optarg);
				break;
			case 'f':
				*(fileName) = optarg;
				break;
			default://give help info
				Help();
				break; 
		}

	}
	//check if arguments are valid 
	int isValid = ValidateInput(*workerAmount, *workerSimLimit, *timeInterval, *fileName);

	if(isValid == 0)
	{//valid arguments 
		return;	
	}
	else
	{
		exit(1);
	}
}
int ValidateInput(int workerAmount, int workerSimLimit, int timeInterval, char* fileName)
{
	//acts a bool	
	int isValid = 0;

	FILE* tstPtr = fopen(fileName, "w");
	//if file fails to open then throw error
	//likely invald extension
	if(tstPtr == NULL)
	{
		printf("\nFailed To Access Input File\n");
		exit(1);
	}
	fclose(tstPtr);
	//arguments cant be negative 
	if(workerAmount < 0 || workerSimLimit < 0 || timeInterval < 0)
	{
		printf("\nInput Arguments Cannot Be Negative Number!\n");	 
		isValid = 1;	
	}
	//args cant be zero
	if(workerAmount < 1)
	{
		printf("\nNo Workers To Launch!\n");
		isValid = 1;
	}
	//no more than 20 workers
	if(workerAmount > 20)
	{
		printf("\nTo many Workers!\n");
		isValid = 1;
	}
	//need to launch 1 worker at a time mun
	if(workerSimLimit < 1)
	{
		printf("\nWe Need To Be Able To Launch At Least 1 Worker At A Time!\n");
		isValid = 1;
	}
	//time interval must be less than a second
	if(timeInterval > MAX_NANOSECOND)
	{
		printf("\nTime Interval must be less than 1 second!\n");
		isValid = 1;
	}
	return isValid;

}

void BuildResourceDescriptor(int resourceDesc[])
{
	for(int i = 0; i < RES_AMOUNT;i++)
	{
		resourceDesc[i] = RES_INSTANCES;
	}	
}
void BuildAllocationTable(int allocTable[TABLE_SIZE][RES_AMOUNT])
{
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		for(int j = 0; j < RES_AMOUNT;j++)
		{
			allocTable[i][j] = 0;
		}

	}
}

int CanGrantResourceClaim(int resourceDesc[], int resourceId)
{//can we give a worker a resource?
	if(resourceDesc[resourceId] <= 0)
	{
		return 0;
	}
	return 1;
}
//when resource is claimed or release
void UpdateResourceDescriptor(int resourceDesc[], int resourceId, int action)
{//availablity vecotr

	if(action == 0)
	{//claim
		resourceDesc[resourceId] = resourceDesc[resourceId] - 1;
	}
	if(action == 1)
	{//release
		resourceDesc[resourceId] = resourceDesc[resourceId] + 1;
	}
}
//when resource is claimed or relesaed
void UpdateAllocationTable(int allocTable[TABLE_SIZE][RES_AMOUNT], int resourceId, int workerIndex, int action)
{
	if(action == 0)
	{//claim
		allocTable[workerIndex][resourceId] =  allocTable[workerIndex][resourceId] + 1;
	}
	if(action == 1)
	{//release
		allocTable[workerIndex][resourceId] = allocTable[workerIndex][resourceId] - 1;
	}	

}
//remeber last resource request incase worker is blocked
void UpdateLastResourceRequest(int workerIndex, int resourceId, struct PCB table[])
{
	table[workerIndex].lastResourceClaim = resourceId;
}
//get all resources back from terminated worker
void GetReleasedResources(int resourceDesc[],int allocTable[TABLE_SIZE][RES_AMOUNT], int workerIndex, FILE* logger){
	LogMessage(logger, "releasing terminated workers resources of ");
	for(int j = 0; j < RES_AMOUNT;j++)
	{
		if(allocTable[workerIndex][j] > 0)
		{
			resourceDesc[j] += allocTable[workerIndex][j];
			allocTable[workerIndex][j] = 0;	
			LogMessage(logger, "R %d",j);
		}

	}
	LogMessage(logger,"\n");
}
int WakeUpProcess(struct PCB table[], int resourceDescriptor[],int allocTable[TABLE_SIZE][RES_AMOUNT], int msqid)
{//wake up processes if their resource is available
	int wokenUp = 0;
	msgbuffer msg;
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].state == STATE_BLOCKED)
		{
			if(resourceDescriptor[table[i].lastResourceClaim] > 0)
			{
				wokenUp++;	 

				ResponseHandler(msqid,table[i].pid, &msg);

				UpdateResourceDescriptor(resourceDescriptor, table[i].lastResourceClaim, 0);

				UpdateAllocationTable(allocTable, table[i].lastResourceClaim, i, 0);

				UpdateWorkerStateInProcessTable(table, table[i].pid, STATE_RUNNING);
			}
		}


	}
	return wokenUp;

}
void BuildBankerTables(int resDesc[], int work[], int finish[])
{//work table and finish table
	for(int i = 0; i < RES_AMOUNT;i++)
	{
		work[i] = resDesc[i];	
	}	
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		finish[i] = 0;
	}

}
void DeadlockHandler(struct PCB processTable[], int resourceDescriptor[], int allocTable[TABLE_SIZE][RES_AMOUNT], FILE* logger)
{
	//step 1
	int work[RES_AMOUNT];
	int finish[RES_AMOUNT];//0 == false, 1 == true
	BuildBankerTables(resourceDescriptor, work, finish);

	for(int i = 0; i < TABLE_SIZE;i++)
	{

		if(processTable[i].state == STATE_TERMINATED)
		{
			finish[i] = 1;
		}
		else
		{
			finish[i] = 0;
		}

	}
	//step 2
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(finish[i] == 0)
		{	
			if(1 <= resourceDescriptor[processTable[i].lastResourceClaim])
			{
				//step 3
				work[i] = work[i] + allocTable[i][processTable[i].lastResourceClaim];	
				finish[i] = 1;
			}
		}	
	}
	int wasDeadlock = 0;
	//step 4

	for(int i = 0 ; i < TABLE_SIZE;i++)
	{
		if(finish[i] == 0)
		{//detected deadlocked worker
			if(processTable[i].pid != 0)
			{
				LogMessage(logger,"detected deadlock process %d. terminating it\n", processTable[i].pid);
				kill(processTable[i].pid, SIGTERM);
				processTable[i].state = STATE_TERMINATED;
				processTable[i].forceTerm = 1;
				wasDeadlock = 1;
				break;
			}
		}
	}
	if(wasDeadlock == 1)
	{//run alg again if deadlock via recursion
		LogMessage(logger, "re running deadlock algorithm\n");
		DeadlockHandler(processTable, resourceDescriptor, allocTable,logger);
	}
	printf("\n");
}

int LogMessage(FILE* logger, const char* format,...)
{//logging to file
	static int lineCount = 0;
	lineCount++;
	if(lineCount > 10000)
	{
		return 1;
	}

	va_list args;

	va_start(args,format);

	vfprintf(logger,format,args);

	va_end(args);

	return 0;
}
void WorkerHandler(int workerAmount, int workerSimLimit,int timeInterval, char* logFile, struct Sys_Time* OsClock, struct PCB processTable[], int resourceDescriptor[], int allocationTable[TABLE_SIZE][RES_AMOUNT])
{	//access logfile
	FILE *logger = fopen(logFile, "w");
	//get id of message queue after creating it
	int msqid = ConstructMsgQueue();

	printf("msg: %d\n", msqid);
	//tracks amount of workers finished
	int workersComplete = 0;

	//tracks amount of workers left to be launched
	int workersLeftToLaunch = workerAmount;

	//amount of workers in ready, blocked, or running state	
	int workersInSystem = 0;
	//increment clock to let us launch first worker
	RunSystemClock(OsClock, timeInterval);
	//holds next time we are allowed to launch a new worker after t time interval		
	int timeToLaunchNxtWorkerSec = OsClock->seconds;
	int timeToLaunchNxtWorkerNano = OsClock->nanoseconds;
	//holds next time we can output porcess table every half seoond
	int timeToOutputSec = 0;
	int timeToOutputNano = 0;
	//calcualtes time to print table for timeToOutput varables
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,HALF_SEC,0, &timeToOutputSec, &timeToOutputNano);
	//time until we run detection alg, every 1 sec
	int timeToDetectSec = 0;
	int timeToDetectNano = 0;
	GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds, 0,1, &timeToDetectSec, &timeToDetectNano);

	msgbuffer msg;
	//data for end of program report	
	int reqWait = 0;
	int reqInst = 0;
	int deadlockRuns = 0;

	//keep looping until all workers (-n) have finished working
	while(workersComplete != workerAmount)
	{
		//if thier are still workers left to launch
		if(workersLeftToLaunch != 0)
		{//if thier are less workers in the system the the allowed simultanous value of amount of workers that can run at once

			if(workersInSystem < workerSimLimit)
			{ 	
				//it timeInterval t has passed
				if(CanEvent(OsClock->seconds, OsClock->nanoseconds, timeToLaunchNxtWorkerSec, timeToLaunchNxtWorkerNano) == 1)
				{	
					//laucnh new worker
					WorkerLauncher(1, processTable, OsClock, logger);

					workersLeftToLaunch--;

					workersInSystem++;

					//determine nxt time to launch worker
					GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,timeInterval,0, &timeToLaunchNxtWorkerSec, &timeToLaunchNxtWorkerNano);

				}
			}
		}		


		//if 1/2 second passed, print process table
		if(CanEvent(OsClock->seconds,OsClock->nanoseconds, timeToOutputSec, timeToOutputNano) == 1)
		{
			PrintAllocationTable(processTable,allocationTable,logger);
			PrintResourceAvailability(resourceDescriptor,logger);
			PrintProcessTable(processTable, OsClock->seconds, OsClock->nanoseconds,logger);
			GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds,HALF_SEC,0, &timeToOutputSec, &timeToOutputNano);
		}


		//send and recieve message to specific worker. returns amount of time worker ran and possibly amount of time it must be blocked for
		int gotMsg = RequestHandler(msqid,&msg);	

		if(gotMsg == 1 && DidWorkerSendRequest(processTable, msg.workerID) == 1)
		{//if we got a worker message

			int canCommitAction = 1;

			if(msg.action == 0)
			{//if worker claims resource
				LogMessage(logger, "Process %d requesting an action on %d at time %d:%d\n", msg.workerID,msg.resourceID, OsClock->seconds, OsClock->nanoseconds);
				UpdateLastResourceRequest(GetWorkerIndexFromProcessTable(processTable, msg.workerID), msg.resourceID,processTable);
				//if the resource the worker wants is not available then canCommitAction becomes zero and os will not grant claim via update
				//
				canCommitAction = CanGrantResourceClaim(resourceDescriptor, msg.resourceID);
				//
			}
			if(canCommitAction == 1)
			{//we can give resource to workeri
				if(msg.action == 1)
				{
					LogMessage(logger, "Process %d released %d at time %d:%d\n", msg.workerID,msg.resourceID, OsClock->seconds, OsClock->nanoseconds);

				}
				else
				{
					LogMessage(logger, "Process %d granted %d at time %d:%d\n", msg.workerID,msg.resourceID, OsClock->seconds, OsClock->nanoseconds);
				}			
				UpdateResourceDescriptor(resourceDescriptor, msg.resourceID, msg.action);
				UpdateAllocationTable(allocationTable, msg.resourceID, GetWorkerIndexFromProcessTable(processTable, msg.workerID), msg.action);

				ResponseHandler(msqid, msg.workerID, &msg);	
				reqInst++;
			}
			if(canCommitAction == 0)
			{
				LogMessage(logger, "Process %d blocked waiting for %d at time %d:%d\n", msg.workerID,msg.resourceID, OsClock->seconds, OsClock->nanoseconds);
				//BLOCK WORKER
				UpdateWorkerStateInProcessTable(processTable, msg.workerID, STATE_BLOCKED);


			}


		}		
		if(CanEvent(OsClock->seconds,OsClock->nanoseconds, timeToDetectSec, timeToDetectNano) == 1)
		{//RUN DEADLOCK ALG	
			LogMessage(logger, "Running deadlock algorithm\n");
			deadlockRuns++;
			DeadlockHandler(processTable, resourceDescriptor, allocationTable, logger);
			GenerateTimeToEvent(OsClock->seconds, OsClock->nanoseconds, 0,1, &timeToDetectSec, &timeToDetectNano);
		}	
		//await that worker toterminate and get its pid
		int workerFinishedId = AwaitWorker();

		if(workerFinishedId != 0)
		{
			workersComplete++;

			UpdateLastResourceRequest(GetWorkerIndexFromProcessTable(processTable, workerFinishedId), -1, processTable);

			LogMessage(logger, "Process %d terminated naturally\n",workerFinishedId);
			GetReleasedResources(resourceDescriptor, allocationTable, GetWorkerIndexFromProcessTable(processTable, workerFinishedId), logger); 


			UpdateWorkerStateInProcessTable(processTable, workerFinishedId, STATE_TERMINATED);

			workersInSystem--;

		}		
		//see if any processes can be awoken 
		reqWait += WakeUpProcess(processTable, resourceDescriptor, allocationTable, msqid);	
		RunSystemClock(OsClock, 500000);
	}

	Report(processTable, reqWait, reqInst, deadlockRuns);
	DestructMsgQueue(msqid);
	fclose(logger);
}
//ensures id from msgrcv is legit
int DidWorkerSendRequest(struct PCB table[], int workerId)
{
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].pid == workerId && workerId != 0)
		{
			return 1;
		}

	}
	return 0;
}
//check if we passed a time for an event
int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark)
{
	if(eventSecMark == curSec && eventNanoMark <= curNano)
	{
		return 1;
	}
	else if(eventSecMark < curSec)
	{
		return 1;	
	}
	else
	{
		return 0;
	}

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
void WorkerLauncher(int amount, struct PCB table[], struct Sys_Time* clock, FILE* logger)
{


	//keep launching workers until limit (is reached
	//to create workers, first create a new copy process (child)
	//then replace that child with a worker program using execlp

	for(int i = 0 ; i < amount; i++)
	{


		pid_t newProcess = fork();




		if(newProcess < 0)
		{
			printf("\nFailed To Create New Process\n");
			exit(1);
		}
		if(newProcess == 0)
		{//child process (workers launching and running their software)

			execlp("./worker", "./worker", NULL);

			printf("\nFailed To Launch Worker\n");
			exit(1);
		}
		//add worker launched to process table 
		AddWorkerToProcessTable(table,newProcess, clock->seconds, clock->nanoseconds);
		LogMessage(logger, "OSS: Generating process with PID %d and putting it in ready queue at time %d:%d\n",newProcess, clock->seconds, clock->nanoseconds);
	}	
	return;
}

int AwaitWorker()
{	
	int pid = 0;

	int stat = 0;
	//nonblocking await to check if any workers are done
	pid = waitpid(-1, &stat, WNOHANG);

	if(pid == -1)
	{
		perror("error occured awaiting worker\n");
		exit(1);
	}	
	if(WIFEXITED(stat)) {

		if(WEXITSTATUS(stat) != 0)
		{
			exit(1);
		}	
	}

	//return pid of finished worker
	//pid will equal 0 if no workers done
	return pid;
}

int GetWorkerIndexFromProcessTable(struct PCB table[], pid_t workerId)
{//find the index (0 - 19) of worker with pid passed in params using table
	for(int i = 0; i < TABLE_SIZE;i++)
	{

		if(table[i].pid == workerId)
		{

			return i;
		}

	}
	printf("Invalid Worker Id: %d\n", workerId);
	while(1)
	{}
}

void AddWorkerToProcessTable(struct PCB table[], pid_t workerId, int secondsCreated, int nanosecondsCreated)
{
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		//look for empty slot of process table bt checking each rows pid	
		if(table[i].pid == 0)
		{

			table[i].pid = workerId;
			table[i].startSeconds = secondsCreated;
			table[i].startNano = nanosecondsCreated;
			table[i].state = STATE_RUNNING;
			//break out of loop, prevents assiginged this worker to every empty row
			break;
		}

	}	
}

void UpdateWorkerStateInProcessTable(struct PCB table[], pid_t workerId, int state)
{ //using pid of worker, update status state of theat work. currently used to set a running workers state to terminated (aka 0 value)

	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].pid == workerId)
		{
			table[i].state = state;
		}
	}
}	
//initalize
void BuildProcessTable(struct PCB table[])
{ 
	for(int i = 0; i < TABLE_SIZE;i++)
	{ //give default values to process table so it doesnt output junk
		table[i].pid = 0;
		table[i].state = 0;
		table[i].startSeconds = 0;
		table[i].startNano = 0;
		table[i].lastResourceClaim = 0;
		table[i].forceTerm = 0;
	}	

}	

//pirnt table
void PrintProcessTable(struct PCB processTable[],int curTimeSeconds, int curTimeNanoseconds, FILE* logger)
{ //printing 
	int os_id = getpid();	
	LogMessage(logger, "\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	LogMessage(logger, "Process Table:\n");
	LogMessage(logger,"      State      PID       StartS       StartN         ResourceReq\n");

	printf("\nOSS PID:%d SysClockS: %d SysclockNano: %d\n",os_id, curTimeSeconds, curTimeNanoseconds);
	printf("Process Table:\n");
	printf("      State      PID       StartS       StartN         ResourceReq\n");
	for(int i = 0; i < TABLE_SIZE; i++)
	{
		if(processTable[i].pid != 0)
		{
			LogMessage(logger, "%d        %d          %d          %d         %d         %d \n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].lastResourceClaim);

			printf("%d        %d          %d          %d         %d         %d \n", i, processTable[i].state, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].lastResourceClaim);

		}
	}	
}
//print allocation table
void PrintAllocationTable(struct PCB processTable[],int allocationTable[TABLE_SIZE][RES_AMOUNT], FILE* logger)
{
	LogMessage(logger, "Allocation Table: \n");
	printf("Allocation Table: \n");	
	for(int i = 0;i < TABLE_SIZE;i++)
	{
		if(processTable[i].pid != 0)
		{	
			LogMessage(logger, "Worker Id: %d Resources: ",processTable[i].pid);
			printf("Worker Id: %d Resources: ", processTable[i].pid);
			for(int j = 0; j < RES_AMOUNT;j++)
			{	
				LogMessage(logger, "R%d: %d ",j, allocationTable[i][j]);
				printf("R%d: %d ",j, allocationTable[i][j]);

			}
			LogMessage(logger, "\n");
			printf("\n");

		}
	}

}
//print resource availbality
void PrintResourceAvailability(int resourceDescriptor[], FILE* logger)
{
	LogMessage(logger, "Available Resources: \n");
	printf("Available Resources: \n");
	for(int i = 0; i < RES_AMOUNT;i++)
	{
		LogMessage(logger,"R%d: %d ", i, resourceDescriptor[i]);
		printf("R%d: %d ", i, resourceDescriptor[i]);

	}
	LogMessage(logger,"\n");
	printf("\n");
}
//print report
void Report(struct PCB table[], int requestWait, int requestInstances,int runs)
{
	printf("\nOS REPORT:\n");
	printf("Requests granted instantly: %d\n",requestInstances);
	printf("Requests granted after wait %d\n",requestWait);
	int countForced = 0;
	for(int i = 0; i < TABLE_SIZE;i++)
	{
		if(table[i].forceTerm == 1)
		{
			countForced++;
		}
	}
	printf("Processes terminated by deadlock handler %d\n",countForced);

	printf("Processes terminated  naturally %d\n",TABLE_SIZE - countForced);

	printf("deadlock handler runs %d\n", runs);

}

//help info
void Help() {
	printf("When executing this program, please provide three numeric arguments");
	printf("Set-Up: oss [-h] [-n proc] [-s simul] [-t time]");
	printf("The [-h] argument is to get help information");
	printf("The [-n int_value] argument to specify the amount of workers to launch");
	printf("The [-s int_value] argument to specify how many workers are allowed to run simultaneously");
	printf("The [-t int_value] argument for a time interval  to specify that a worker will be launched every t amount of nanoseconds. (MUST BE IN NANOSECONDS)");
	printf("The [-f string_value] argument is used to specify the file name of the file that will be used as log a file");
	printf("Example: ./oss -n 5 -s 3 -t 7 -f test.txt");
	printf("\n\n\n");
	exit(1);
}
