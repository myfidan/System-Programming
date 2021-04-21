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
#define FIFO_PERM (S_IRUSR | S_IWUSR)

char* findFifo(char* addr, char* fifoFileBuffer);
char* find_write_fifo(char* addr);
int take_potato_id(char* readFifo);
void update_shr_mem(char* addr,int potato_id);

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

    char fifoFileBuffer[BUFF_SIZE];
    fifoFileBuffer[0] = '\0';
    int fd;
    while(((fd = open(filewithfifonames,O_RDONLY)) == -1) && (errno == EINTR));
    if(fd == -1){
    	perror("open file error");
    	return 1;
    }
    char c;
    int readFifoCount;
    while((readFifoCount = read(fd,&c,1))){
    	strncat(fifoFileBuffer,&c,1);
    }

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

    sem_t* fifo_sema;
    fifo_sema = sem_open("/fifosem",O_CREAT,0666,0);
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

  
    char delimeter = '-';
    char* processFifoName;
    if(check_shm_stat.st_size == 0){ //first time shared memory open
    	ftruncate(fd_shm,BUFF_SIZE*4);
		addr = mmap(NULL,BUFF_SIZE*4, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm,0);
		
		processFifoName = findFifo(addr,fifoFileBuffer);

		memcpy(addr,processFifoName,strlen(processFifoName)+1);
		strncat(addr,&delimeter,1);
    } 
    else{	//not first time
    	addr = mmap(NULL,check_shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm,0);

    	processFifoName = findFifo(addr,fifoFileBuffer);


		strcat(addr,processFifoName);
		strncat(addr,&delimeter,1);
    	//write(STDOUT_FILENO,addr,check_shm_stat.st_size);
    	//strcat(addr,x);
    }

   	//Add potato id and number to shared memory
   	char current_potato_id[10];
   	current_potato_id[0] = '\0';
   	snprintf(current_potato_id,10,"%ld",(long)getpid());
   	strncat(current_potato_id,&delimeter,1);

    if(haspotatoornot > 0){
    	char appendPotato[20];
    	appendPotato[0] = '\0';
    	snprintf (appendPotato, 20, "%ld_%d", (long)getpid(), haspotatoornot);
    	strncat(appendPotato,&delimeter,1);
    	strcat(addr,appendPotato);
    }
   
    //create fifo
    if(mkfifo(processFifoName,FIFO_PERM) == -1){
    	if(errno != EEXIST){
    		perror("fifo error");
    		return 1;
    	}
    }
    //critical section //
    sem_post(sema);


    
    int fifoFd;
    if(haspotatoornot > 0){ // if process has potato
    	
    	sem_wait(fifo_sema);
    	char* find_fifo_for_write = find_write_fifo(addr);
    	//delete last character R
    	find_fifo_for_write[strlen(find_fifo_for_write)-1]= '\0';


		
    	while(((fifoFd = open(find_fifo_for_write,O_WRONLY)) == -1) && (errno == EINTR));
    	if(fifoFd == -1){
    		perror("open fifo error");
    		return 1;
    	}
    	printf("%ld sending potato number %d to %s\n",(long)getpid(),take_potato_id(current_potato_id) ,find_fifo_for_write);
    	printf("addr = %s\n",addr );
    	write(fifoFd,current_potato_id,strlen(current_potato_id));

    }
    else{
    	//add his fifo to shared memory to find a write process

    	sem_wait(sema); // block shared memory because I will use it
    	// /home/cse312/Desktop/fifos/fifo2-/home/cse312/Desktop/fifos/fifo2R-
    	char fifo_read_name[200];
    	fifo_read_name[0] = '\0';
    	char readDelimeter = 'R';
    	strcat(fifo_read_name,processFifoName);
    	strncat(fifo_read_name,&readDelimeter,1);
    	strncat(fifo_read_name,&delimeter,1);
    	strcat(addr,fifo_read_name);

    	sem_post(sema); // unblock shared memory my work is done with it

    	sem_post(fifo_sema);
    	
    	while(((fifoFd = open(processFifoName,O_RDONLY)) == -1) && (errno == EINTR));
    	if(fifoFd == -1){
    		perror("open fifo error");
    		return 1;
    	}
    	char readFifo[20];
    	readFifo[0] = '\0';
    	char car;
    	do{
    		read(fifoFd,&car,1);
    		strncat(readFifo,&car,1);
    	}while(car != '-');
    	int potato_id = take_potato_id(readFifo);
    	printf("%ld receiving potato number %d from %s\n", (long)getpid(),potato_id,processFifoName);

    	//decrease 1 from shared memory for this potato
    	//critical section
    	sem_wait(sema);
    	update_shr_mem(addr,potato_id);
    	sem_post(sema);
    	printf("addr = %s\n",addr );
    }

    //end critical section
    if(close(fd_shm) == -1){	// fd_shm is no longer needed  
    	perror("close error");
    	return 1;
    }

  
    //close and unlink shm
    //close and unlink sema
    //munmap(addr,check_shm_stat.st_size);
    //shm_unlink(nameofsharedmemory);
    //sem_close(sema);
    //sem_unlink(namedsemaphore);

    //sem_close(fifo_sema);
    //sem_unlink("/fifosem");
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

char* find_write_fifo(char* addr){
	char* tempstr = calloc(strlen(addr)+1, sizeof(char));
	strcpy(tempstr, addr);

	const char s[2] = "-";
	char *token;
	token = strtok(tempstr, s);


	while( token != NULL ){
	    if(token[strlen(token)-1] == 'R'){
	    	free(tempstr);
	    	return token;
	    }

	    token = strtok(NULL, s);
    }

	free(tempstr);
	return NULL;
}

int take_potato_id(char* readFifo){
	return atoi(readFifo);
}

void update_shr_mem(char* addr,int potato_id){
	char* tempstr = calloc(strlen(addr)+1, sizeof(char));
	strcpy(tempstr, addr);

	char* newAddr = calloc(strlen(addr)+1, sizeof(char));
	newAddr[0] = '\0';

	char potatoID[10];
	char potatoSwitch[10];

	const char s[2] = "-";
	const char s2[2] = "_";
	char del = '-';
	char del2 = '_';
	char *token;
	token = strtok(tempstr, s);


	while( token != NULL ){
	    if(token[0] >= '0' && token[0] <= '9'){
	    	char tempstr2[20];
	    	tempstr2[0] = '\0';
	    	strcpy(tempstr2,token);

	    	char* token2;
	    	token2 = strtok(tempstr2,s2); //pid
	    	strcpy(potatoID,token2);
	    	if(atoi(potatoID) == potato_id){
	    		strcat(newAddr,token2);
	    		strncat(newAddr,&del2,1);

		    	token2 = strtok(NULL,s2);	//number of swtich needed for potato
		    	int num_switch = atoi(token2);
		    	num_switch--;
		    	char newSwitch[10];
		    	snprintf(newSwitch,10,"%d",num_switch);
		    	strcat(newAddr,newSwitch);
		    	strncat(newAddr,&del,1);
	    	}
	    	else{
	    		strcat(newAddr,token2);
	    		strncat(newAddr,&del2,1);

	    		token2 = strtok(NULL,s2);	//number of swtich needed for potato
		    	strcpy(potatoSwitch,token2);
		    	strcat(newAddr,token2);
		    	strncat(newAddr,&del,1);
	    	}
	    	token = strtok(NULL, s);
	    }
	    else{
	    	strcat(newAddr,token);
	    	strncat(newAddr,&del,1);
	    	token = strtok(NULL, s);
	    }
    }
    strcpy(addr,newAddr);
	free(tempstr);
}