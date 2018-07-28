#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>	

	
#define MAX_CLIENTS 10            //Max number of clients the server can process 
#define BUFSIZE 100               //Max line number of the text file, meaning 
#define MAX_LINE 1024				 //Max length of a single line in the text file
#define MAX_KEYWORD_SIZE 128		 //Max length of a keyword input from the user          
#define MAX_SHM_SEM_NAME 128	    //Max shared memory and prefix names length
#define MAX_PREFIX 128

//12 semaphores: 10 for result queues, 1 for requests array and 1 for queue_state.
sem_t* mutex[12];
sem_t* empty[11];
sem_t* full[11];	 

struct request {
	int  index;
	char keyword[MAX_KEYWORD_SIZE];
};

struct result {
	int buf[BUFSIZE];
	int res_in;
	int res_out;
	int count;
};

//Structure of shared memory
struct shared_memory {
	//About requests
	int queue_state[MAX_CLIENTS];
	struct request requests[MAX_CLIENTS];
	int req_in;
   int req_out;
   int count;
	
	//About results
	struct result results[MAX_CLIENTS];		
}; 

//Define a pointer to this struct
struct shared_memory *sp;

int main(int argc, char *argv[]) {	 
	 //Name of shared memory
	 char shm_name[MAX_SHM_SEM_NAME];
	 strcpy(shm_name, argv[1]);
	 
	 //Access the shared memory segment
	 int shm_fd;
	 void *ptr;
	 
	 //Integer value to store the index of the queue that will be used to retrieve results
	 int index = -1;
	 
	 shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
	 ftruncate(shm_fd, sizeof(struct shared_memory));
	 ptr = mmap(0,sizeof(struct shared_memory), PROT_WRITE, MAP_SHARED, shm_fd, 0);
	
	 //This is the pointer that accesses the shared memory region
	 sp = (struct shared_memory *) ptr;

	 //Used to open semaphores 
	 char prefix[MAX_PREFIX];
	 char mt[MAX_PREFIX*2];
	 char em[MAX_PREFIX*2];
	 char fu[MAX_PREFIX*2];
	 strcpy(prefix, argv[3]);

    //Open mutex semaphore for queue state	     
	 snprintf(mt, sizeof(mt), "%s%s", prefix, "-mutex-queue-state");
	 	 
	 //Mutex semaphore for request queue
	 if ((mutex[11] = sem_open(mt, O_RDWR)) == SEM_FAILED)
	     perror ("sem_open");
	 //printf("sem %s created\n", mt);
	 
	 sem_wait(mutex[11]);
		 for(int i = 0; i < MAX_CLIENTS; i++) {
		 	if(sp->queue_state[i] == 0) {
		 		sp->queue_state[i] = 1;
		 		index = i;
		 		break;
		 	}
		 }
		 if(index == -1) {
		 	printf("too	 many	 clients	 started\n");
		 	exit(EXIT_FAILURE);
		 }
	 sem_post(mutex[11]);
	 
	 //printf("index is %d for %s \n", index, argv[1]);
	 
	 for(int i = 0; i < 10; i++) {
		snprintf(mt, sizeof(mt), "%s%s%d", prefix, "-mutex-result", i);
		snprintf(em, sizeof(mt), "%s%s%d", prefix, "-empty-result", i);
		snprintf(fu, sizeof(mt), "%s%s%d", prefix, "-full-result", i);
	
		 //Mutex semaphore
		 if ((mutex[i] = sem_open (mt, O_RDWR)) == SEM_FAILED)
		     perror ("sem_open");

		 //Full semaphore
		 if ((full[i] = sem_open (fu, O_RDWR)) == SEM_FAILED)
		     perror ("sem_open");
		        
		 //Empty semaphore
		 if ((empty[i] = sem_open (em, O_RDWR)) == SEM_FAILED)
		     perror ("sem_open");
	}	 
	
	 //Initialize request queue semaphores	     
	 snprintf(mt, sizeof(mt), "%s%s", prefix, "-mutex-request");
    snprintf(em, sizeof(mt), "%s%s", prefix, "-empty-request");
    snprintf(fu, sizeof(mt), "%s%s", prefix, "-full-request"); 
	 	 
	 //Mutex semaphore for request queue
	 if ((mutex[10] = sem_open(mt, O_RDWR)) == SEM_FAILED)
	     perror ("sem_open");

	 //Full semaphore for request queue
	 if ((full[10] = sem_open(fu, O_RDWR)) == SEM_FAILED)
	     perror ("sem_open");
	        
	 //Empty semaphore for request queue
	 if ((empty[10] = sem_open(em, O_RDWR)) == SEM_FAILED)
	     perror ("sem_open"); 
	  
	 //Generate the request
	 struct request *input;
	 input = malloc(sizeof(struct request));
	 input->index = index;
	 strcpy(input->keyword, argv[2]);
	 
    //Send the request to the request queue using also semaphores for safety
	 sem_wait(empty[10]);
	 sem_wait(mutex[10]);
	 	sp->requests[sp->req_in] = *input;
	 	sp->req_in = (sp->req_in + 1) % MAX_CLIENTS;	
	 sem_post(mutex[10]);
	 sem_post(full[10]);

	 int num = -2;
	 
	 //Printing loop
	 while(num != -1) {	 		
		 //CRITICAL SECTION
		 sem_wait(full[index]);
		 sem_wait(mutex[index]);			 	 
			 //Read and print an item from the buffer
			 num = sp->results[index].buf[sp->results[index].res_out];     	 
			 sp->results[index].res_out = (sp->results[index].res_out + 1) % BUFSIZE;
			 if(num != -1) {
			 	printf("%d\n", num);
			 }	 		
		 sem_post(mutex[index]);
		 sem_post(empty[index]); 		 
	 }
	 
	 //Reset queue state at index = 0
    sem_wait(mutex[11]);
    sp->queue_state[index] = 0;
    sem_post(mutex[11]);
    
    //Close semaphores
    sem_close(mutex[index]);
    sem_close(empty[index]);
    sem_close(full[index]);
    sem_close(mutex[11]);				
	
   return 0;
}
