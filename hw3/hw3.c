#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>
#include <getopt.h>

#include <semaphore.h>

#define BUFF_SIZE 1024

int main(int argc, char* argv[]){

	int opt;
	int hasporaroornot;
	char* nameofsharedmemory;
	char* filewithfifonames;
	char* namedsemaphore;
	int checkArg = 0;
	extern char *optarg;
	while((opt = getopt(argc, argv, "b:s:f:m:")) != -1)  
    {  
        switch(opt){
        	case 'b':
        		hasporaroornot = atoi(optarg);
        		checkArg++;
        	break;

        	case 's':
        		nameofsharedmemory = optarg;
        		checkArg++;
        	break;
        	
        	case 'f':
        		filewithfifonames = optarg;
        		checkArg++;
        	break;
        	
        	case 'm':
        		namedsemaphore = optarg;
        		checkArg++;
        	break;

        	default:
        		printf("Argument Error\n");
        		return 1;
        } 
    }    
    if(checkArg != 4) {
    	printf("Argument Error\n");
    	return 1;
    }

    char fifoFileBuffer[1024];
    int fd;
    while(((fd = open(filewithfifonames,O_RDONLY)) == -1) && (errno == EINTR));
    if(fd == -1){
    	perror("open file error");
    	return 1;
    }
    int readFifoCount = read(fd,fifoFileBuffer,BUFF_SIZE);
    if(readFifoCount == -1){
    	perror("Failed to read fifo file");
    	return 1;
    }

    if(close(fd) == -1){
    	perror("Failed to close file");
    	return 1;
    }

    //create named semaphore
    sem_t* sema;
    sema = sem_open(namedsemaphore,O_CREAT,0666,1);
    //sem wait
    sem_wait(sema);
    //critical section //

    //critical section //
    sem_post(sema);
    //end critical section

    printf("%d\n", hasporaroornot );
    printf("%s\n",nameofsharedmemory );
    printf("%s\n",filewithfifonames );
    printf("%s\n",namedsemaphore );
    printf("-------------------\n");
    printf("%s\n",fifoFileBuffer );
    
    sem_close(sema);
    sem_unlink(namedsemaphore);
	return 0;
}