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
#include <sys/mman.h>
#define BUFF_SIZE 1024

char* findFifo(char* addr, char* fifoFileBuffer);

int main(int argc, char* argv[]){

	int opt;
	int haspotatoornot;
	char* nameofsharedmemory;
	char* filewithfifonames;
	char* namedsemaphore;
	int checkArg = 0;
	extern char *optarg;
	while((opt = getopt(argc, argv, "b:s:f:m:")) != -1)  
    {  
        switch(opt){
        	case 'b':
        		haspotatoornot = atoi(optarg);
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

    //Shared memory create
    

    //sem wait
    sem_wait(sema);
    //critical section //


    int fd_shm = shm_open(nameofsharedmemory, O_CREAT | O_RDWR,0666);
    if(fd_shm < 0){
    	perror("shm_open error");
    	return 1;
    }
    struct stat check_shm_stat;
    char* addr;

    if(fstat(fd_shm,&check_shm_stat) == -1){
    	perror("fstat error");
    	return 1;
    }

    char* x = "abc";
    char delimeter = '-';
    char* processFifoName;
    if(check_shm_stat.st_size == 0){ //first time shared memory open
    	printf("shm size 0\n");
    	ftruncate(fd_shm,BUFF_SIZE*4);
		addr = mmap(NULL,BUFF_SIZE*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm,0);
		
		processFifoName = findFifo(addr,fifoFileBuffer);
		strncat(processFifoName,&delimeter,1);

		printf("%s\n",processFifoName);
		memcpy(addr,processFifoName,strlen(processFifoName)+1);

    } 
    else{	//not first time
    	printf("shm size not 0\n");
    	addr = mmap(NULL,check_shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm,0);

    	processFifoName = findFifo(addr,fifoFileBuffer);
		strncat(processFifoName,&delimeter,1);

		printf("%s\n",processFifoName);

		strcat(addr,processFifoName);

    	//write(STDOUT_FILENO,addr,check_shm_stat.st_size);
    	//strcat(addr,x);
    }

   	//Add potato id and number to shared memory
    if(haspotatoornot > 0){
    	char appendPotato[20];
    	appendPotato[0] = '\0';
    	snprintf (appendPotato, 20, "*%ld_%d*", (long)getpid(), haspotatoornot);
    	strncat(appendPotato,&delimeter,1);
    	strcat(addr,appendPotato);
    }
   
    printf("%s\n",addr );

    //critical section //
    sem_post(sema);
    //end critical section
    if(close(fd_shm) == -1){	// fd_shm is no longer needed  
    	perror("close error");
    	return 1;
    }

  
    //close and unlink shm
    //close and unlink sema
    munmap(addr,check_shm_stat.st_size);
    shm_unlink(nameofsharedmemory);
    sem_close(sema);
    sem_unlink(namedsemaphore);
	return 0;
}



char* findFifo(char* addr, char* fifoFileBuffer){
	char* findedFifoName;
	char* ret;
	const char s[2] = "\n";
	char *token;
	token = strtok(fifoFileBuffer, s);

	while( token != NULL ){
	    ret = strstr(addr,token);
	    if(ret == NULL){
	    	findedFifoName = token;
	    	break;
	    }

	    token = strtok(NULL, s);
    }

    return findedFifoName;
   
}