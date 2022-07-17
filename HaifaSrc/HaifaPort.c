#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include<string.h>
#include <time.h>    //for using the time as the seed to strand!

#define True 1
#define False 0
#define BUFFER_SIZE 50
#define MAX_SLEEP_TIME 3000
#define MIN_SLEEP_TIME 5

// instance variables
int numberOfVessels;
HANDLE* vesselsThreads;
HANDLE* vesselsSemaphore;
int* vesselsID;
HANDLE suezMutex, rndMutex;



HANDLE StdinRead, StdinWrite;      /* pipe for writing parent to child */
HANDLE StdoutRead, StdoutWrite;    /* pipe for writing child to parent */
STARTUPINFO si;
PROCESS_INFORMATION pi;
char message[BUFFER_SIZE];
DWORD read, written, ThreadId;
BOOL success;
TCHAR ProcessName[256];

// Methods' declrations 
DWORD WINAPI runVessel(PVOID);
void checkArgument();
int initGlobalData();
int calcSleepTime();
char* getTimeAsString();
void freeAll();
void SuazCanelToEilatSide(int id);

int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        printf("ERROR: You need to enter an argument that will be the number of vessels\n");
        return 0;
    }

    if (argc > 2)
    {
        printf("ERROR: You need to enter only 1 argument\n");
        return 0;
    }
    numberOfVessels = atoi(argv[1]);
    checkArgument(numberOfVessels);
    printf("Haifa port - The number of vessels that you entered as argument is: %d\n", numberOfVessels);


    /* set up security attributes so that pipe handles are inherited */
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };

    /* allocate memory */
    ZeroMemory(&pi, sizeof(pi));

    /* create the pipe for writing from parent to child */
    if (!CreatePipe(&StdinRead, &StdinWrite, &sa, 0)) {
        fprintf(stderr, "Create Pipe Failed\n");
        return 1;
    }

    /* create the pipe for writing from child to parent */
    if (!CreatePipe(&StdoutRead, &StdoutWrite, &sa, 0)) {
        fprintf(stderr, "Create Pipe Failed\n");
        return 1;
    }

    /* establish the START_INFO structure for the child process */
    GetStartupInfo(&si);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /* redirect the standard input to the read end of the pipe */
    si.hStdOutput = StdoutWrite;
    si.hStdInput = StdinRead;
    si.dwFlags = STARTF_USESTDHANDLES;

    wcscpy(ProcessName, L"..\\..\\EilatPort\\Debug\\EilatPort.exe");

    /* create the child process */
    if (!CreateProcess(NULL,
        ProcessName,
        NULL,
        NULL,
        TRUE, /* inherit handles */
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        fprintf(stderr, "Process Creation Failed\n");
        return -1;
    }





    sprintf(message, "%d", numberOfVessels);
    printf("Haifa port - Requesting from Eilat port a permission to send %d vessels \n", numberOfVessels);
    /* the parent now wants to write to the pipe */
    if (!WriteFile(StdinWrite, message, BUFFER_SIZE, &written, NULL))
        printf("Error writing to pipe haifa port\n");



    /* now read from the pipe */
    success = ReadFile(StdoutRead, message, BUFFER_SIZE, &read, NULL);
    if (success) {
        if (message[0] == 'E') {
            printf("Haifa port - The permission to sail %d vessels was denied by Eilat port \n", numberOfVessels);
            return -1;
        }
        printf("Haifa port - The permission to sail %d vessels was given by Eilat port \n", numberOfVessels);
    }
    else {
        printf("Error reading from pipe at haifa port\n");
        return -1;
    }
    if (initGlobalData() == False)
        exit(1);
    int i;
    for (i = 0; i < numberOfVessels; i++)
    {
        vesselsID[i] = i + 1;
        vesselsThreads[i] = CreateThread(NULL, 0, runVessel, &vesselsID[i], 0, &ThreadId);
        if (vesselsThreads[i] == NULL)
        {
            printf("Unexpected Error in Thread %d Creation at haifa port\n", (i+1));
            return 1;
        }
    }

    while (ReadFile(StdoutRead, message, BUFFER_SIZE, &read, NULL)) {

        if (message[0] != 'E') {
            int temp = atoi(message);
            printf("%s Vessel %d - done sailing @ Haifa Port \n",getTimeAsString(), temp);
            Sleep(calcSleepTime());
            
            if (!ReleaseSemaphore(vesselsSemaphore[temp-1], 1, NULL)) {
                printf("Error while releasing the semaphore of vessel %d\n",temp);
                //exit?
            }
            i--;
            if (i == 0)
                break;
        }

    }
    WaitForMultipleObjects(numberOfVessels, vesselsThreads, TRUE, INFINITE);
    printf("%s Haifa Port: All Vessels Threads are done\n", getTimeAsString());

    /* wait for the child to exit */
    WaitForSingleObject(pi.hProcess, INFINITE);
    freeAll();
    printf("%s Haifa Port: Exiting...\n", getTimeAsString());
}

void checkArgument()
{
    if (numberOfVessels < 2 || numberOfVessels>50)
    {
        printf("Error! the number of vessels should be between 2 to 50");
        exit(1);
    }
}

//A method that initalize all the global data of the port (arrays, semaphores and mutexs)
int initGlobalData()
{
    vesselsID = (int*)malloc(numberOfVessels * sizeof(int));
    vesselsThreads = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));
    vesselsSemaphore = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));
    if (vesselsID == NULL || vesselsThreads == NULL || vesselsSemaphore == NULL)
    {
        printf("Error acquired while the malloc has been done");
        return False;
    }
    rndMutex = CreateMutex(NULL, FALSE, NULL);
    if (rndMutex == NULL)
    {
        return False;
    }
    suezMutex = CreateMutex(NULL, FALSE, NULL);
    if ((suezMutex) == NULL)
    {
        printf("Error acquired while the mutex of suez canel was created");
        return False;
    }

    for (int i = 0; i < numberOfVessels; i++)
    {
        vesselsSemaphore[i] = CreateSemaphore(NULL, 0, 1, NULL);
        if (vesselsSemaphore[i] == NULL)
        {
            printf("Error acquired while create the vessels semaphore");
            return False;
        }
    }
    return True;
}

//Illustrate a lifecycle of the vessel in Haifa Port
DWORD WINAPI runVessel(PVOID Param)
{
    srand((unsigned int)time(NULL));
    int vesselID = *(int*)Param;
    printf("%s Vessel %d - starts sailing @ Haifa Port\n", getTimeAsString(), vesselID);

    //Sleep time between MIN_SLEEP_TIME and MAX_SLEEP_TIME msec
    Sleep(calcSleepTime());
    SuazCanelToEilatSide(vesselID);

    return 0;
}

//generic function to randomise a Sleep time between MIN_SLEEP_TIME and MAX_SLEEP_TIME msec
int calcSleepTime()
{
    int res;
    WaitForSingleObject(rndMutex, INFINITE);
    res = rand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME)+ MIN_SLEEP_TIME+1;
    if (!ReleaseMutex(rndMutex))
        printf("Haifa port - calcSleepTime::Unexpected error rndMutex.V()\n");
    return res;
}

//return a string that represent the time now in a format of [HH:MM:SS]
char* getTimeAsString()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timeString[11];
    sprintf(timeString,"[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return timeString;
}

//a method that illustrate the way from Haifa to Eilat through Suez canel
void SuazCanelToEilatSide(int id)
{
    WaitForSingleObject(suezMutex, INFINITE);
    printf("%s Vessel %d - entering Canel: Med. Sea ==> Red Sea\n", getTimeAsString(), id);
    Sleep(calcSleepTime());
    sprintf(message, "%d", id);
    if (!WriteFile(StdinWrite, message, sizeof(message), &written, NULL))
        printf("Error writing to pipe\n");
    if (!ReleaseMutex(suezMutex))
        printf("Error while releasing to suez canel mutex\n");
    WaitForSingleObject(vesselsSemaphore[id], INFINITE);
}

//Free memory and close all the handles after all the vessels finished at Haifa
void freeAll()
{
    for (int i = 0; i < numberOfVessels; i++)
    {
        CloseHandle(vesselsSemaphore[i]);
        if (!CloseHandle(vesselsThreads[i]) && !CloseHandle(vesselsSemaphore[i])) {
            printf("Error! Closing a handle of the vessels Threads or their Semaphore had been failed");
            return False;
        }
    }
    if (!CloseHandle(suezMutex) && !CloseHandle(StdoutWrite) && !CloseHandle(StdinRead) && !CloseHandle(rndMutex) && !CloseHandle(StdinWrite) && !CloseHandle(StdoutRead) && !CloseHandle(pi.hProcess) && !CloseHandle(pi.hThread)) {
        printf("Error! At least one of the handles had been failed while been closed");
        return False;
    }
    free(vesselsID);
}
