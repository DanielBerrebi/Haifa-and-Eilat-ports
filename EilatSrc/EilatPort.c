#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <windows.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#define True 1
#define False 0
#define BUFFER_SIZE 50
#define MAX_SLEEP_TIME 3000
#define MIN_SLEEP_TIME 5
#define MAX_WEIGHT 50
#define MIN_WEIGHT 5
#define MIN_CRANES 2

// instance variables
int numberOfVessels;
int numberOfCranes;
int vesselsAtBarrier;
int emptyCranes;
int waitingVessels;
HANDLE* vesselsThreads;
HANDLE* vesselsMutex;
HANDLE* cranesThreads;
HANDLE* cranesSemaphore;
HANDLE waitingSemaphore;
int* vesselsID;
int* cranesID;
int* cranesUsedArray;
int* vesselsWeight;
HANDLE suezMutex;
HANDLE barrierSemaphore;
HANDLE unloadingQuaySemaphore;
HANDLE rndMutex;
HANDLE ReadHandle, WriteHandle;
CHAR buffer[BUFFER_SIZE];
DWORD read, written,ThreadId;;
BOOL success;

// Methods' declrations 
DWORD WINAPI runCrane(PVOID Param);
DWORD WINAPI runVessel(PVOID);
int initGlobalData();
bool isNotPrime();
char* getTimeAsString();
void freeAll();
void startCranes();
int calcSleepTime();
void randomNumberOfCranes();
int getWeight();
void SuazCanelToHaifaSide(int id);
void barrier(int vesselID);
void adt(int vesselID, int queuePlace);

int main(VOID)
{
    ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
    WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    // now have the child read from the pipe
    success = ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL);

    // we have to output to stderr as stdout is redirected to the pipe
    if (success) 
    {
        numberOfVessels=atoi(buffer);
        fprintf(stderr, "Eilat port - Haifa port sent a request to send us %d vessels\n", numberOfVessels);
        if (!isNotPrime())
        {
            buffer[0] = 'E';//E means Error
            fprintf(stderr, "Eilat port - The permission is denied because the number of vessels is a prime number\n");
            // Now write amended string  to the pipe
            if (!WriteFile(WriteHandle, buffer, BUFFER_SIZE, &written, NULL))
                fprintf(stderr, "Error writing to pipe\n");
        }
        else
            fprintf(stderr, "Eilat port - The permission to sail %d vessels as been given\n",numberOfVessels);
    }
    else 
    {
        fprintf(stderr, "Error writing to pipe\n");
        return -1;
    }
    if (initGlobalData() == False)
        buffer[0] = 'E';//E means Error

    // Now write amended string  to the pipe
    if (!WriteFile(WriteHandle, buffer, BUFFER_SIZE, &written, NULL))
        fprintf(stderr, "Error writing to pipe\n");

    int i = 0;
    while (ReadFile(ReadHandle, buffer, BUFFER_SIZE, &read, NULL)) 
    {
        int temp = atoi(buffer) -1;
        vesselsID[temp] = temp + 1;
        fprintf(stderr,"%s Vessel %d - arrived @ Eilat Port\n", getTimeAsString(), (temp + 1));
        Sleep(calcSleepTime());
        vesselsThreads[temp] = CreateThread(NULL, 0, runVessel, &vesselsID[temp], 0, &ThreadId);
        if (vesselsThreads[temp] == NULL)
        {
            fprintf(stderr,"Unexpected Error in vessel %d Creation at eilat port\n", (temp + 1));
            buffer[0] = 'E';    //E means Error
            if (!WriteFile(WriteHandle, buffer, BUFFER_SIZE, &written, NULL))
                fprintf(stderr, "Error writing to pipe\n");
            return 1;
        }
        i++;
        if (i == numberOfVessels)
            break;
    }

    WaitForMultipleObjects(numberOfCranes, cranesThreads, TRUE, INFINITE);
    fprintf(stderr, "%s Eilat Port: All Cranes Threads are done\n", getTimeAsString());
    WaitForMultipleObjects(numberOfVessels, vesselsThreads, TRUE, INFINITE);
    fprintf(stderr, "%s Eilat Port: All Vessels Threads are done\n", getTimeAsString());
    freeAll();
    fprintf(stderr, "%s Eilat Port: Exiting...\n", getTimeAsString());
    return 0;
}
    
//Check if the number that we got from Haifa port isn't a prime number
bool isNotPrime()
{
    for (int i = 2; i <= sqrt(numberOfVessels); i++)
    {
        if (numberOfVessels % i == 0)
        {
            return true;
        }
    }
    return false;
}

//Free memory and close all the handles after all the vessels sailed back to Haifa
void freeAll()
{
    if (!CloseHandle(suezMutex) && !CloseHandle(rndMutex) && !CloseHandle(barrierSemaphore) && !CloseHandle(waitingSemaphore) && !CloseHandle(ReadHandle) && !CloseHandle(WriteHandle))
    {
        fprintf(stderr, "Error! At least one of the handles had been failed while been closed");
        return False;
    }
    for (int i = 0; i < numberOfVessels; i++)
    {
        if (!CloseHandle(vesselsThreads[i]) && !CloseHandle(vesselsMutex[i])) 
        {
            fprintf(stderr, "Error! At least one of the handles had been failed while been closed");
            return False;
        }
    }
    for (int i = 0; i < numberOfCranes; i++)
    {
        if (!CloseHandle(cranesThreads[i]) && !CloseHandle(cranesSemaphore[i])) 
        {
            fprintf(stderr, "Error! At least one of the handles had been failed while been closed");
            return False;
        }
    }
    free(vesselsID);
    free(cranesID);
    free(cranesUsedArray);
    free(vesselsWeight);
}

//A method that initalize all the global data of the port (arrays, semaphores and mutexs)
int initGlobalData()
{
    rndMutex = CreateMutex(NULL, FALSE, NULL);
    if (rndMutex == NULL)
    {
        return False;
    }
    srand((unsigned int)time(NULL));
    randomNumberOfCranes();
    vesselsID = (int*)malloc(numberOfVessels * sizeof(int));
    vesselsThreads = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));
    vesselsMutex = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));
    vesselsWeight = (int*)malloc(numberOfVessels * sizeof(int));
    cranesID = (int*)malloc(numberOfCranes * sizeof(int));
    cranesUsedArray = (int*)malloc(numberOfCranes * sizeof(int));
    cranesThreads = (HANDLE*)malloc(numberOfCranes * sizeof(HANDLE));
    cranesSemaphore = (HANDLE*)malloc(numberOfCranes * sizeof(HANDLE));
 

    if (vesselsID == NULL || vesselsThreads == NULL || vesselsMutex == NULL|| vesselsWeight == NULL || cranesID == NULL || cranesThreads == NULL || cranesUsedArray == NULL || cranesSemaphore == NULL)
    {
        fprintf(stderr, "Error acquired while the malloc has been done\n");
        return False;
    }
    suezMutex = CreateMutex(NULL, FALSE, NULL);
    if ((suezMutex) == NULL)
    {
        fprintf(stderr, "Error acquired while the mutex of suez canel was created\n");
        return False;
    }
    unloadingQuaySemaphore = CreateSemaphore(NULL, 0, numberOfCranes, NULL);
    if (unloadingQuaySemaphore == NULL)
    {
        fprintf(stderr, "Error acquired while the unloadingQuaySemaphore Semaphore was created\n");
        return 1;
    }
    for (int i = 0; i < numberOfVessels; i++)
    {
        vesselsMutex[i] = CreateMutex(NULL, FALSE, NULL);
        if (vesselsMutex[i] == NULL)
        {
            fprintf(stderr, "Error acquired while the vesselsMutex mutex of vessel %d was created\n",i+1);
            return False;
        }
    }
    for (int i = 0; i < numberOfCranes; i++)
    {
        cranesSemaphore[i] = CreateSemaphore(NULL, 0, 1, NULL);
        if (cranesSemaphore[i] == NULL)
        {
            fprintf(stderr, "Error acquired while the cranesSemaphore Semaphore of crane %d was created\n", i + 1);
            return False;
        }
    }
    barrierSemaphore = CreateSemaphore(NULL, 0, numberOfVessels, NULL);
    if (barrierSemaphore == NULL)
    {
        fprintf(stderr, "Error acquired while the barrier semaphore was created\n");
        return False;
    }
    waitingSemaphore = CreateSemaphore(NULL, numberOfCranes, numberOfCranes, NULL);
    if (barrierSemaphore == NULL)
    {
        fprintf(stderr, "Error acquired while the waiting semaphore was created\n");

    }
    for (int i = 0; i < numberOfCranes; i++)
    {
        cranesUsedArray[i] = FALSE;
    }
    emptyCranes = numberOfCranes;
    startCranes();
    return True;
}

//Illustrate a lifecycle of the vessel in Eilat Port
DWORD WINAPI runVessel(PVOID Param)
{
    srand((unsigned int)time(NULL));
    int vesselID = *(int*)Param;
    barrier(vesselID);
    SuazCanelToHaifaSide(vesselID);
    return 0;
}

//The barrier is responible to send an amount of vessels that equal to the number of crane at the same time
//to the unloading dock until all the vessels passed the barrier
void barrier(int vesselID)
{
    fprintf(stderr, "%s Vessel %d - is arrived at the barrier\n", getTimeAsString(), vesselID);
    int queuePlace = ++vesselsAtBarrier;
    waitingVessels++;
    WaitForSingleObject(waitingSemaphore, INFINITE);
    if (waitingVessels >= numberOfCranes && queuePlace % numberOfCranes == 0)
    {
        waitingVessels -= numberOfCranes;
        if (!ReleaseSemaphore(barrierSemaphore, numberOfCranes, NULL))
            fprintf(stderr, "Error acquired while the barrier semaphore was released\n");
    }
    WaitForSingleObject(barrierSemaphore, INFINITE);
    adt(vesselID, queuePlace);
}

//Responible to match each vessel to an available crane then to check each vessel cargo weight and unpack it
void adt(int vesselID, int queuePlace)
{
    fprintf(stderr, "%s Vessel %d - is entering to the Unloading Quay \n", getTimeAsString(), vesselID);
    Sleep(calcSleepTime());
    cranesUsedArray[queuePlace % numberOfCranes] = vesselID;
    int craneID = (queuePlace % numberOfCranes) + 1;
    fprintf(stderr, "%s Vessel %d - is arrived to crane %d \n", getTimeAsString(), vesselID, craneID);
    vesselsWeight[vesselID - 1] = getWeight();
    fprintf(stderr, "%s Vessel %d - has a weight of %d tons \n", getTimeAsString(), vesselID, vesselsWeight[vesselID - 1]);
    if (!ReleaseSemaphore(cranesSemaphore[craneID - 1], 1, NULL))
        fprintf(stderr, "Error acquired while the cranesSemaphore %d of crane was released\n", craneID);
    WaitForSingleObject(unloadingQuaySemaphore, INFINITE);
    Sleep(calcSleepTime());
    fprintf(stderr, "%s Vessel %d - has exited from the ADT %d \n", getTimeAsString(), vesselID, craneID);
    if (cranesUsedArray[craneID-1] != -1)
        cranesUsedArray[craneID-1] = FALSE;
    emptyCranes--;
    if (emptyCranes == 0) {
        if (!ReleaseSemaphore(waitingSemaphore, numberOfCranes, NULL))
            fprintf(stderr, "Error acquired while waitingSemaphore was released\n");
        emptyCranes = numberOfCranes;
    }

}

//generic function to randomise a Sleep time between MIN_SLEEP_TIME and MAX_SLEEP_TIME msec
int calcSleepTime()
{
    int res;
    WaitForSingleObject(rndMutex, INFINITE);
    res = rand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME) + MIN_SLEEP_TIME + 1;
    if (!ReleaseMutex(rndMutex))
        fprintf(stderr, "EilatPort - calcSleepTime::Unexpected error rndMutex.V()\n");
    return res;
}


//a method that create the cranes threads
void startCranes()
{
    for (int i = 0; i < numberOfCranes; i++)
    {
        cranesID[i] = i + 1;
        cranesThreads[i] = CreateThread(NULL, 0, runCrane, &cranesID[i], 0, &ThreadId);
        if (cranesThreads[i] == NULL)
        {
            fprintf(stderr, "Unexpected Error in crane %d Creation\n", (i + 1));
            buffer[0] = 'E';//E means Error
            if (!WriteFile(WriteHandle, buffer, BUFFER_SIZE, &written, NULL))
                fprintf(stderr, "Error writing to pipe\n");
        }
    }
    
}

//Illustrate a lifecycle of a crane that is job is to unpack a cargo 
DWORD WINAPI runCrane(PVOID Param)
{
    srand((unsigned int)time(NULL));
    int craneID = *(int*)Param;
    int counter = 0;
    while(True)
    { 
        WaitForSingleObject(cranesSemaphore[craneID-1], INFINITE);
        int vessel = cranesUsedArray[craneID - 1];
        fprintf(stderr, "%s Crane %d - is stated to unpack a cargo that weight %d tons \n", getTimeAsString(), craneID, vesselsWeight[vessel-1]);
        Sleep(calcSleepTime());
        fprintf(stderr, "%s Crane %d - is finished to unpack a cargo that weight %d tons \n", getTimeAsString(), craneID, vesselsWeight[vessel - 1]);
        if (!ReleaseSemaphore(unloadingQuaySemaphore, 1, NULL))
            fprintf(stderr, "Error acquired while unloadingQuaySemaphore was released\n");
        if (numberOfVessels / numberOfCranes == ++counter)
        {
            cranesUsedArray[craneID - 1] = -1;
            fprintf(stderr, "%s Crane %d - is finished his work \n", getTimeAsString(), craneID);
            break;
        }
    }
    return 0;
}

//return a string that represent the time now in a format of [HH:MM:SS]
char* getTimeAsString()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timeString[11];
    sprintf(timeString, "[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return timeString;
}

//choosing a random number that divide the number of vessels without a remainder
void randomNumberOfCranes()
{

    do 
    {
        WaitForSingleObject(rndMutex, INFINITE);
        numberOfCranes = rand() % (numberOfVessels - MIN_CRANES - 1) + MIN_CRANES;
        if (!ReleaseMutex(rndMutex))
            fprintf(stderr,"EilatPort - randomNumberOfCranes::Unexpected error rndMutex.V()\n");
    } while (numberOfVessels % numberOfCranes != 0);
}

//generic function to randomise a weight for a vessel that is between MIN_WEIGHT and MAX_WEIGHT tons
int getWeight()
{
    int weight;
    WaitForSingleObject(rndMutex, INFINITE);
    weight = rand() % (MAX_WEIGHT - MIN_WEIGHT) + MIN_WEIGHT;
    if (!ReleaseMutex(rndMutex))
        fprintf(stderr, "EilatPort - getWeight::Unexpected error rndMutex.V()\n");
    return weight;
}

//a method that illustrate the way from Eilat to Haifa through Suez canel
void SuazCanelToHaifaSide(int id)
{
    WaitForSingleObject(suezMutex, INFINITE);
    fprintf(stderr, "%s Vessel %d - entering Canel: Red Sea ==> Med. Sed\n", getTimeAsString(), id);
    Sleep(calcSleepTime());
    fprintf(stderr, "%s Vessel %d - exiting Canel: Red Sea ==> Med. Sed\n", getTimeAsString(), id);
    if (!ReleaseMutex(suezMutex))
        fprintf(stderr, "Error acquired while suez canel mutex was released\n");
    sprintf(buffer, "%d", id);
    if (!WriteFile(WriteHandle, buffer, BUFFER_SIZE, &written, NULL))
        fprintf(stderr, "Error writing to pipe\n");
}