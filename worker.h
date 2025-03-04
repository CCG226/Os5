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

void TaskHandler();

struct Sys_Time* AccessSystemTime();

void DisposeAccessToShm(struct Sys_Time* clock);

int AccessMsgQueue();


void SendRequest(int msqid, msgbuffer *msg, int resourceId, int requestAction);


int GetResourceResponse(int msqid, msgbuffer *msg);

int CanEvent(int curSec,int curNano,int eventSecMark,int eventNanoMark);

int GenerateRequestTime();

int ShouldTerminate();

void GenerateTimeToEvent(int currentSecond,int currentNano,int timeIntervalNano,int timeIntervalSec, int* eventSec, int* eventNano);

int ClaimResource(int resourceArray[]);
int ReleaseResource(int resourceArray[]);
void AppendResource(int resourceArray[], int id);
int GenerateRequestTime();
int AllResourcesClaimed(int resourceArray[]);
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
