struct Sys_Time {
int seconds;
int nanoseconds;
int rate;

};
typedef struct msgbuffer {
long mtype;
int resourceID;  
int action;
int workerID;
} msgbuffer;
//sets up worker behaviour
//which is to wait for os to send request to run with timeslice, then call SelectTask()
//after a task has been picked (run, terminate, access external resource) send a response to the os with details on its task
void TaskHandler();

//if worker blocked by external resource it must wait for
//generate random time worker must 'wait' for as double between 0 - 5 seconds +  0 - 1000 miliseconds 
double GenerateEventWaitTime();
//accesses shared memory resource for system clock
struct Sys_Time* AccessSystemTime();

//detaches worker from shared memory system clock resource after terminating 
void DisposeAccessToShm(struct Sys_Time* clock);

//accesses message queue create by os to communicate to os
int AccessMsgQueue();

//worker waits for os to send request message with timeslice value to indicate worker can for timeslcie amount of time
void SendRequest(int msqid, msgbuffer *msg, int resourceId, int requestAction);

//send resposne to os indicating how much time it ran and the type of task it did
int GetResourceResponse(int msqid, msgbuffer *msg);

int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark);

int GenerateRequestTime();

int ShouldTerminate();

void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano);

int ClaimResource();
int ReleaseResource(int resourceArray[]);
void AppendResource(int resourceArray[], int id);
int GenerateRequestTime();

int ClaimOrReleaseResource();

//if worker is in operation
const int RUNNING = 1;
//if worker is terminating and ending operations
const int TERMINATING = 0;

int RES_AMOUNT = 10;

const int MAX_NANOSECOND = 1000000000;

const int REQUEST_BOUND = MAX_NANOSECOND / 10;

const int MSG_SYSTEM_KEY = 5303;
const int SYS_TIME_SHARED_MEMORY_KEY = 63131;
