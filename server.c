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
#include <semaphore.h>
#include <unistd.h>

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

//Text file name
char text_file[MAX_SHM_SEM_NAME];

struct request{
	int  index;
	char keyword[MAX_KEYWORD_SIZE];
};

struct result{
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

void *read_from_file(void* param) {
	 struct request *my_req = (struct request*)param;
	 
	 FILE* file = fopen(text_file, "r"); //Text file which will be accessed from threads
    char line[MAX_LINE]; 
    int i = 0; //Line count
	 
    while (fgets(line, sizeof(line), file)) {
        i++;
        char *pch = strstr(line,(char *) my_req->keyword); //Check if keyword is in the line
        //printf("%d,  %s", i, line); 
        if(pch) {       	  
        	  //CRITICAL SECTION TO ADD LINE TO RESULT QUEUE
        	  sem_wait(empty[my_req->index]);
        	  sem_wait(mutex[my_req->index]);
        	  
        	 	  //Add the value to the buffer
        	  	  sp->results[my_req->index].buf[sp->results[my_req->index].res_in] = i;
        	  	  sp->results[my_req->index].res_in = (sp->results[my_req->index].res_in + 1) % BUFSIZE ;
			  	
			  //EXIT FROM CRITICAL SECTION
           sem_post(mutex[my_req->index]);
           sem_post(full[my_req->index]);
        }        
    } 
    
	 //CRITICAL SECTION TO ADD -1
	 sem_wait(empty[my_req->index]);
	 sem_wait(mutex[my_req->index]);
	 
   	 sp->results[my_req->index].buf[sp->results[my_req->index].res_in] = -1;
   	 sp->results[my_req->index].res_in = (sp->results[my_req->index].res_in + 1) % BUFSIZE ;
   	 
    //EXIT FROM CRITICAL SECTION
    sem_post(mutex[my_req->index]);
    sem_post(full[my_req->index]);
	   
	 //Close the file and exit thread
    fclose(file);
	 pthread_exit(0);
} 
	

int main(int argc, char *argv[]) {
	 //Variables needed for shared memory segment
	 int shm_fd;
	 void *ptr;
    
    //Name of text file
    strcpy(text_file, argv[2]);
     
	 //Name of shared memory
	 char shm_name[MAX_SHM_SEM_NAME];
	 strcpy(shm_name, argv[1]);
	 
	 shm_unlink(shm_name);     //Unlink to be safe
	 shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
	 ftruncate(shm_fd, sizeof(struct shared_memory));
	 ptr = mmap(0,sizeof(struct shared_memory), PROT_WRITE, MAP_SHARED, shm_fd, 0);
	 sp = (struct shared_memory *) ptr;
	 close(shm_fd); //File descriptor no longer needed
	 //Memory is already created and allocated and we can access it from sp pointer

	//Initialize everything
	for(int i = 0; i < MAX_CLIENTS; i++){
		sp->results[i].res_in = 0;
		sp->results[i].res_out = 0;
		sp->results[i].count = 0;
		sp->queue_state[i]= 0;
		strcpy( sp->requests[i].keyword , "");
		sp->requests[i].index = -3;
		for(int j = 0; j < BUFSIZE; j++) {
			sp->results[i].buf[j] = 0;
		}
	}	
	sp->count = 0;
	sp->req_in = 0;
	sp->req_out = 0;
	
	
	//Used to open semaphores 
	char prefix[MAX_PREFIX];
	char mt[MAX_PREFIX*2];
	char em[MAX_PREFIX*2];
	char fu[MAX_PREFIX*2];
	strcpy(prefix, argv[3]);

	for(int i = 0; i < 10; i++) {
		snprintf(mt, sizeof(mt), "%s%s%d", prefix, "-mutex-result", i);
		snprintf(em, sizeof(em), "%s%s%d", prefix, "-empty-result", i);
		snprintf(fu, sizeof(fu), "%s%s%d", prefix, "-full-result", i);
		sem_unlink(mt); //Unlink if it exists
		sem_unlink(em); //Unlink if it exists
		sem_unlink(fu); //Unlink if it exists

		//Mutex semaphore
		 if ((mutex[i] = sem_open (mt, O_RDWR | O_CREAT, 0660, 1)) == SEM_FAILED)
			  perror ("sem_open");
		 //Full semaphore
		 if ((full[i] = sem_open (fu, O_RDWR | O_CREAT, 0660, 0)) == SEM_FAILED)
			  perror ("sem_open");			     
		 //Empty semaphore
		 if ((empty[i] = sem_open (em, O_RDWR | O_CREAT, 0660, BUFSIZE)) == SEM_FAILED)
			  perror ("sem_open");	
	}
	
	 //Initialize request queue semaphores	     
	 snprintf(mt, sizeof(mt), "%s%s", prefix, "-mutex-request");
	 snprintf(em, sizeof(mt), "%s%s", prefix, "-empty-request");
	 snprintf(fu, sizeof(mt), "%s%s", prefix, "-full-request"); 
	 sem_unlink(mt); //Unlink if it exists
	 sem_unlink(em); //Unlink if it exists
	 sem_unlink(fu); //Unlink if it exists	 	 
	 	 
	 //Mutex semaphore for request queue
	 if ((mutex[10] = sem_open (mt, O_RDWR | O_CREAT, 0660, 1)) == SEM_FAILED)
		  perror ("sem_open");
	 //Full semaphore for request queue
	 if ((full[10] = sem_open (fu, O_RDWR | O_CREAT, 0660, 0)) == SEM_FAILED)
		  perror ("sem_open");			  
	 //Empty semaphore for request queue
	 if ((empty[10] = sem_open (em, O_RDWR | O_CREAT, 0660, MAX_CLIENTS)) == SEM_FAILED)
		  perror ("sem_open");	 
			  
	 //Initialize queue_state mutex semaphore	  
	 snprintf(mt, sizeof(mt), "%s%s", prefix, "-mutex-queue-state");
	 sem_unlink(mt); //Unlink if it exists 
	 
	 if ((mutex[11] = sem_open (mt, O_RDWR | O_CREAT, 0660, 1)) == SEM_FAILED)
		  perror ("sem_open");
	  	 
	  	 
	// printf("in is %d\n", sp->req_in);	
	// printf("out is %d\n", sp->req_out);	
	 
	// INFINITE LOOP	 
	while(1) {	 	
		pthread_t tid; // Different thread IDs
		pthread_attr_t attr; // Different set of thread attributes

		struct request *input; 
		input = malloc(sizeof(struct request));
		
		sem_wait(full[10]);
		sem_wait(mutex[10]);		 		 
		//We are ready to create the thread
		//printf("Thread is ready to execute\n");   
		input = &sp->requests[sp->req_out];
	
		//printf("Keyword %s is ready for queue %d \n", input->keyword, input->index);
		sp->req_out = (sp->req_out + 1) % 10;
	
		//printf("in is %d\n", sp->req_in);	
		//printf("out is %d\n", sp->req_out);	 		  
		sem_post(mutex[10]);
		sem_post(empty[10]);

	   //Create thread							 	 
		pthread_attr_init(&attr); 
		pthread_create(&tid, &attr, read_from_file, (void *)input);
		//pthread_join(tid, NULL);
	} 
	return 0;
}
