#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h> 
#include <errno.h>
#include <signal.h> 
#include <string>
#include <iostream>
#include <semaphore.h>
#include <fcntl.h>

//const int MEMORY_SIZE = 4096;
//const int BUFFER_sIZE = 256;
int x = 0;
sem_t* cpMutex;	// Semaphore for signal handler - Implemented to avoid race conditions among processes

// Signal handler for the child process 
void childHandler(int sig){
	x = 1;
}
// Signal handler for the parent process 
void parentHandler(int sig){
	x += 1;
	sem_post(cpMutex);
}
// Signal handler2 for the parent process
void parentHandler2(int sig){
	x = 0;
}
// Function to print an int array 
void printArray (int arr[], int size) 
{
	int i;

	for(i = 0; i < size; i++)
	{
		printf("%d ", arr[i]);
	}

	printf("\n");
}

// Prefix sum -> Requires sharedArray, tempArray, number of elements and number of processors to do the work
int prefixSum(int* sharedArray, int* tempArray, int numElements, int numProcessors) {

	int logProc = ((int) ceil(log(numElements) / log(2))) - 1;	// Calculate the number of iterations 
	int loopSize = numElements / numProcessors;			// Calculate the number of internal loops
	int i, j, k, neighborDistance, pid, segment_id, sem_id;		
	// Shared between processes
	// Declare semaphore, intilizing semaphore 	
	sem_t* mutex;
	mutex = sem_open("/main_sem", O_CREAT, 0644, 1);
	cpMutex = sem_open("/cp_sem", O_CREAT, 0644, 0);

	// Putting doneCount into shared memory 
	int* doneCount;	
	segment_id = shmget (IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
	doneCount = (int*) shmat(segment_id, NULL, 0);
	*doneCount =  0;

	// Create n number of processes

	for(k = 1; k < numProcessors; k++){
		
		pid = fork();
		if(pid == 0){
			signal(SIGINT, childHandler);		// Giving children the signal handler
			mutex = sem_open("/main_sem", O_RDWR);	// Referencing semaphore from parent process
			cpMutex = sem_open("/cp_sem", O_RDWR);
			break;
		}
	}

	k = k % numProcessors;	// Divide k to give each a unique ID
	

	// Outter iteration loop
	for(i = 0; i <= logProc; i++) {
		// Inner loop iterates from 0 -> loopSize
		for(j=0; j < loopSize; j++) {
						
			int index = k * loopSize + j;	// Adjusting index for elements

			if(index < pow(2, i)) {
				tempArray[index] = sharedArray[index];
			} else {
				neighborDistance = index - pow(2, i);	// Calculating distance of neighbor from current index
				tempArray[index] =  sharedArray[neighborDistance] + sharedArray[index];
				
			}
		}
		
		// Critical Section for counting number of processors done
		//printf("INC DC PID=%d\n", getpid());

		// Only allow one child to access at a time
		sem_wait(mutex);
		*doneCount = *doneCount + 1;	// Child process has finished, increment done counter
		sem_post(mutex);


		while(*doneCount != numProcessors) {
		//	printf("DoneCount: %d\n", *doneCount); 	// Processes waiting for everyone to complete
		}
		
		if(pid == 0){					// Sending signal to parent to increment value x (creating a pool of processes that are completed)
			sem_wait(mutex);
			//printf("K=%d, x=%d\n", k, x);
			kill(getppid(), SIGUSR1);
			sem_wait(cpMutex);			// Wait for parent handler to finish
			sem_post(mutex);

			while(x == 0){				// Spin child processes
				// LOOP
				
			}
			x = 0;					
		} else {
			while(x != numProcessors - 1) { 	// Spin parent till all child processes are done
				
			}

			sem_wait(mutex);			// Reset shared doneCount for next iteration, reset child x values to 0
			*doneCount = 0;
			kill(0, SIGINT);
			sem_post(mutex);
			x = 0;
		}
		
		// Swap temp and shared arrays 
		int* temp = sharedArray;
		sharedArray = tempArray;
		tempArray = temp; 

	}
	// Kill all children once all iterations are complete
	if (pid == 0)
	{

		kill(getpid(), SIGKILL);

	}
	
}

int main (int argc, char* argv[])
{

	FILE *inputptr, *outputptr;
	char* inFile;
	char* outFile;
	char buff[255];
	int sizeofArray, i, segment_id_1, segment_id_2, numProcessors;
	//char buffer[BUFFER_sIZE];
	double iterations;

	sizeofArray = atoi(argv[1]);
	inFile = argv[2];
	outFile = argv[3];
	numProcessors = atoi(argv[4]);
	
	int originalArray [sizeofArray];
	int* tempArray;
	int* sharedArray;

	signal(SIGUSR1, parentHandler);		// Defining signal handlers 
	signal(SIGINT, parentHandler2);

//	iterations = log10((double) numProcessors);

//	printf("Log: %f\n", iterations);

	

	if((inputptr = fopen(inFile, "r")) == NULL)
	{
		fprintf(stderr, "Unable to open file"); 
		return -1;
	}


	for (i = 0; i < sizeofArray; i++)
	{

		fscanf(inputptr, "%d", &originalArray[i]);

	}


	fclose(inputptr);

	// The shared memory is created by generating a segment id using shmget, it has a 
	// private key value, the memory is twice the size of the array of ints and it gives 
	// read/write permissions to all clients on the server

	segment_id_1 = shmget( IPC_PRIVATE, (sizeofArray * sizeof(int)), IPC_CREAT | 0666);
	segment_id_2 = shmget( IPC_PRIVATE, (sizeofArray *  sizeof(int)), IPC_CREAT | 0666);
	

	if (segment_id_1 < 0 || segment_id_2 < 0) 
	{
		fprintf(stderr, "Error in creating shared memory segment\n");
		return -1;

	}



	//printf("Segment id = %d\n", segment_id);
	// Arrays in shared memory for processes
	sharedArray = (int*) shmat(segment_id_1 , NULL, 0);
	tempArray = (int*) shmat(segment_id_2, NULL, 0);

	//Populating arrays
	for (i = 0; i < sizeofArray; i++)
	{

		sharedArray[i] = originalArray[i];
		tempArray[i] = originalArray[i];

	}

	// Calls the prefix sum function on the array passing the two arrays in shared memory 
	// (one containing original values), the size of the arrays and the number of processors
	// being used in parallel to compute the prefix sum

	prefixSum(sharedArray, tempArray, sizeofArray, numProcessors);
	
	printf("Processing complete: Results written to output.txt \n");
	
	int logProc = ((int) ceil(log(sizeofArray) / log(2))) -1;	// Calculating the number of iterations



	if ((outputptr = fopen(outFile, "w")) == NULL)
	{
		printf("Unable to open output file");
		return 1; 
	}
	
	// Storing the array into an output file
	if (logProc % 2 == 1)
	{						// Case where sharedArray is final array
		for(i = 0; i < sizeofArray; i++)
		{	
			if (i == sizeofArray - 1)
			{
				fprintf(outputptr, "%d\n", sharedArray[i]);
			} else
			{
				fprintf(outputptr, "%d ", sharedArray[i]);
			}

		}
	} else 						// Case where tempArray is final array
	{
		for(i = 0; i < sizeofArray; i++)
		{

			if(i == sizeofArray -1)
			{
				fprintf(outputptr, "%d\n", tempArray[i]);
			} else
			{

				fprintf(outputptr, "%d ", tempArray[i]);
			}

		}
	}

	// Close the output file
	fclose(outputptr);



	return 0; 
}


