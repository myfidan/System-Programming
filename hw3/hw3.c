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
char hold[20];

char* findFifo(char* addr, char* fifoFileBuffer);
char* find_write_fifo(char* addr);
int take_potato_id(char* readFifo);
void update_shr_mem(char* addr,int potato_id);
void delete_opened_write_fifo(char* addr,char* find_fifo_for_write);
int check_potato_cooldown(char* addr);
char* parse_fifo_name(char* fifo_path);


sig_atomic_t flag = 0;

void handler(int signal_number){
	++flag;
}

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


    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = &handler;
    sigaction(SIGINT,&sa,NULL);

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
    	int switch_num = 1;
    	while(1){
    		

	    	sem_wait(fifo_sema);

	    	sem_wait(sema);
	    	char* find_fifo_for_write = find_write_fifo(addr);
	    	//delete last character R
	    	find_fifo_for_write[strlen(find_fifo_for_write)-1]= '\0';
	    	sem_post(sema);
	    	


			
	    	while(((fifoFd = open(find_fifo_for_write,O_WRONLY)) == -1) && (errno == EINTR));
	    	if(fifoFd == -1){
	    		perror("open fifo error");
	    		return 1;
	    	}
	    	
	    	sem_wait(sema);
	    	
	    	printf("%ld sending potato number %d to %s;this is switch number %d\n",(long)getpid(),take_potato_id(current_potato_id) ,parse_fifo_name(find_fifo_for_write),switch_num);
	   		switch_num += 2;
	   		sem_post(sema);

	    	sem_wait(sema);
	    	delete_opened_write_fifo(addr,find_fifo_for_write);
	    	sem_post(sema);
	    	if(flag > 0){
	    		printf("CTRL C signal arrive\n");
	    		char* sigCatch = "***-";
	    		write(fifoFd,sigCatch,strlen(sigCatch));
	    		break;
	    	}
	    	write(fifoFd,current_potato_id,strlen(current_potato_id));



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
	    	//After send potato now wait for potato
	    	//open own fifo for read and wait
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
	    	if(strcmp(readFifo,"***-") == 0) break;
	    	int potato_id = take_potato_id(readFifo);
	    	if(potato_id == 0) break; //means read EOF
	    	printf("%ld receiving potato number %d from %s\n", (long)getpid(),potato_id,parse_fifo_name(processFifoName));

	    	//decrease 1 from shared memory for this potato
	    	//critical section
	    	sem_wait(sema);
	    	update_shr_mem(addr,potato_id);
	    	sem_post(sema);
	    	sem_wait(sema);
    		int check = check_potato_cooldown(addr);
    		sem_post(sema);
    		if(check == 0){
    			break;
    		}

	    	current_potato_id[0] = '\0';
	    	snprintf (current_potato_id, 10, "%d",potato_id );
	    	strncat(current_potato_id,&delimeter,1);
    	}
    	
    }
    else{
    	int switch_num = 2;
    	//add his fifo to shared memory to find a write process
    	do{

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

	    	if(strcmp(readFifo,"***-") == 0) break;
	    	int potato_id = take_potato_id(readFifo);
	    	if(potato_id == 0) break; //means read EOF   
	    	printf("%ld receiving potato number %d from %s\n", (long)getpid(),potato_id,parse_fifo_name(processFifoName));

	    	//decrease 1 from shared memory for this potato
	    	//critical section
	    	sem_wait(sema);
	    	update_shr_mem(addr,potato_id);
	    	sem_post(sema);

	    	sem_wait(sema);
    		int check = check_potato_cooldown(addr);
    		sem_post(sema);
    		if(check == 0){
    			break;
    		}
	    	sem_wait(fifo_sema);

	    	//Now find a waiting fifo and send potato to him
	    	sem_wait(sema);
	    	char* find_fifo_for_write = find_write_fifo(addr);
	    	//delete last character R
	    	find_fifo_for_write[strlen(find_fifo_for_write)-1]= '\0';
	    	sem_post(sema);
	    	


			
	    	while(((fifoFd = open(find_fifo_for_write,O_WRONLY)) == -1) && (errno == EINTR));
	    	if(fifoFd == -1){
	    		perror("open fifo error");
	    		return 1;
	    	}

	    	current_potato_id[0] = '\0';
	 	  	snprintf(current_potato_id,10,"%ld",(long)potato_id);
	   		strncat(current_potato_id,&delimeter,1);

	   		sem_wait(sema);
	    	printf("%ld sending potato number %d to %s; this is switch number %d\n",(long)getpid(),take_potato_id(current_potato_id) ,parse_fifo_name(find_fifo_for_write),switch_num);
	    	switch_num += 2;
	   		sem_post(sema);

	    	sem_wait(sema);
	    	delete_opened_write_fifo(addr,find_fifo_for_write);
	    	sem_post(sema);
	    	if(flag > 0){
	    		printf("CTRL C signal arrive\n");
	    		char* sigCatch = "***-";
	    		write(fifoFd,sigCatch,strlen(sigCatch));
	    		break;
	    	}
	    	write(fifoFd,current_potato_id,strlen(current_potato_id));
			

	    	

    	}while(1);
    }

    //end critical section
    if(close(fd_shm) == -1){	// fd_shm is no longer needed  
    	perror("close error");
    	return 1;
    }

    if(haspotatoornot > 0 && check_potato_cooldown(addr) == 0){
    	printf("pid=%ld; potato number %ld has cooled down\n",(long)getpid(),(long) getpid());
    }
  
    //close and unlink shm
    //close and unlink sema
    munmap(addr,check_shm_stat.st_size);
    shm_unlink(nameofsharedmemory);
    sem_close(sema);
    sem_unlink(namedsemaphore);

    sem_close(fifo_sema);
    sem_unlink("/fifosem");
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


	const char s[2] = "-";
	char del = '-';
	char del2 = '_';
	char *token;
	token = strtok(tempstr, s);


	while( token != NULL ){
	    if(token[0] >= '0' && token[0] <= '9'){
	    	char total[20];
	    	total[0] = '\0';
	    	char firstpart[10];
	    	firstpart[0] = '\0';
	    	char secondpart[10];
	    	secondpart[0] = '\0';
	    	int j = 0;
	    	int i = 0;
	    	for(i=0; i<strlen(token); i++){
	    		if(token[i] == '_') j = 1;
	    		if(j == 0){
	    			strncat(firstpart,&token[i],1);
	    		}
	    		else{
	    			if(token[i] != '_')
	    			strncat(secondpart,&token[i],1);
	    		}
	    	}
	    	strcat(total,firstpart);
	    	strncat(total,&del2,1);
	    	if(atoi(firstpart) == potato_id){
	    		int number = atoi(secondpart);
		    	number--;
		    	snprintf(secondpart,10,"%d",number);
		    	
	    	}
	    	
	    	strcat(total,secondpart);
	    	strncat(total,&del,1);
	    	strcat(newAddr,total);
	    }
	    else{
	    	strcat(newAddr,token);
	    	strncat(newAddr,&del,1);
	    	
	    }
    	token = strtok(NULL, s);
    }
    strcpy(addr,newAddr);
    //printf("After addr = %s\n", addr);
    free(newAddr);
	free(tempstr);
}


void delete_opened_write_fifo(char* addr,char* find_fifo_for_write){

	char* tempstr = calloc(strlen(addr)+1, sizeof(char));
	strcpy(tempstr, addr);

	char* newAddr = calloc(strlen(addr)+1, sizeof(char));
	newAddr[0] = '\0';

	char* token;
	const char s[2] = "-";
	char del = '-';

	token = strtok(tempstr, s);

	while( token != NULL ){

		if(token[strlen(token)-1] != 'R'){
			strcat(newAddr,token);
			strncat(newAddr,&del,1);
		}
		token = strtok(NULL, s);
	}
	
	strcpy(addr,newAddr);
	free(newAddr);
	free(tempstr);
}


int check_potato_cooldown(char* addr){
	char* tempstr = calloc(strlen(addr)+1, sizeof(char));
	strcpy(tempstr, addr);


	const char s[2] = "-";
	
	char *token;
	token = strtok(tempstr, s);
	int check = 0;
	while( token != NULL ){
	    if(token[0] >= '0' && token[0] <= '9'){
	    	char firstpart[10];
	    	firstpart[0] = '\0';
	    	char secondpart[10];
	    	secondpart[0] = '\0';
	    	int j = 0;
	    	int i = 0;
	    	for(i=0; i<strlen(token); i++){
	    		if(token[i] == '_') j = 1;
	    		if(j == 0){
	    			strncat(firstpart,&token[i],1);
	    		}
	    		else{
	    			if(token[i] != '_')
	    			strncat(secondpart,&token[i],1);
	    		}
	    	}
	    	
	    	int number = atoi(secondpart);
		    if(number != 0){
		    	check = 1;
		    	break;
		    }

	    }
	    
    	token = strtok(NULL, s);
    }

	free(tempstr);
	return check;
}


char* parse_fifo_name(char* fifo_path){
	char* tempstr = calloc(strlen(fifo_path)+1, sizeof(char));
	strcpy(tempstr, fifo_path);

	char* token;
	const char s[2] = "/";
	hold[0] = '\0';
	token = strtok(tempstr, s);

	while( token != NULL ){

		strcpy(hold,token);
		token = strtok(NULL, s);
	}

	free(tempstr);
	return hold;
}

