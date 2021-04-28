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


sig_atomic_t flag = 0;

void handler(int signal_number){
	++flag;
}


void insert_buffer(char* addr,char chr);
void remove_1and2(char* addr);
char remove_buffer(char* addr);

int main(int argc, char* argv[]){

	int n,v,c,b,t;
	char* inputfile;
	int checkArg = 0;
	int opt;

	//Handler for ctrl + c
	struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = &handler;
    sigaction(SIGINT,&sa,NULL);

	extern char *optarg;
	//take arguments from user by using getopt
	while((opt = getopt(argc, argv, "n:v:c:b:t:i:")) != -1)  
    {  
        switch(opt){
        	case 'n':
        		n = atoi(optarg);
        		checkArg++;
        	break;

        	case 'v':
        		v = atoi(optarg);
        		checkArg++;
        	break;
        	
        	case 'c':
        		c = atoi(optarg);
        		checkArg++;
        	break;
        	
        	case 'b':
        		b = atoi(optarg);
        		checkArg++;
        	break;

        	case 't':
        		t = atoi(optarg);
        		checkArg++;
        	break;

        	case 'i':
        		inputfile = optarg;
        		checkArg++;
        	break;
        	
        	default:
        		printf("Argument Error\n");
        		return 1;
        } 
    } 
    //Check arguments correct or not  
    if(checkArg != 6){
    	printf("Wrong argument list\n");
    	return 1;
    }
    if(n < 2 || v < 2 || c < 3 || t < 1 || b < (t*c+1)){
    	printf("Wrong argument values\n");
    	return 1;
    }

    //create shared memory for vaccine buffer
    int fd_shm = shm_open("/vaccineBuf", O_CREAT | O_RDWR,0666);
    if(fd_shm < 0){
    	perror("shm_open error");
    	return 1;
    }
    ftruncate(fd_shm,b);//create b size buffer
    char* addr = mmap(NULL,b, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm,0); 
    //init all space with '-' character it means no vaccine
    for(int i=0; i<b; i++){
    	addr[i] = '-';
    }
    //binary semo for access shared memory
    sem_t* mshare;
    mshare = sem_open("/mshare",O_CREAT,0666,1);
    //Check how many elements in buffer
    sem_t* full;
    full = sem_open("/full",O_CREAT,0666,0);
    //Check buffer Empty or not
    sem_t* empty;
    empty = sem_open("/empty",O_CREAT,0666,b);
    //semaphore for '1' and '2'
    sem_t* m1;
    m1 = sem_open("/m1",O_CREAT,0666,0);
    sem_t* m2;
    m2 = sem_open("/m2",O_CREAT,0666,0);
    
    //processes
    pid_t nurse[n];
    pid_t vaccinator[v];
    pid_t citizen[c];

    //open file
    int fd;
    while(((fd = open(inputfile,O_RDONLY)) == -1) && (errno == EINTR));
    if(fd == -1){
    	perror("open file error");
    	return 1;
    }

    //create Nurses
    for(int i=0; i<n; i++){

    	nurse[i] = fork();
    	//check fork error
    	if(nurse[i] == -1){
    		perror("Error in nurse fork");
    		exit(1);
    	}
    	if(nurse[i] == 0){ // Nurse code
    		int count = 1;
    		char chr;
    		while((count = read(fd,&chr,sizeof(chr)))){
    			if(chr != '\n'){
    				//accessing shared memory lock
    				sem_wait(empty);
    				sem_wait(mshare);
					insert_buffer(addr,chr);

					if(chr == '1') sem_post(m1);
					if(chr == '2') sem_post(m2);
					int valuem1,valuem2;
					sem_getvalue(m1,&valuem1);
					sem_getvalue(m2,&valuem2);	
				    printf("Nurse %d (pid=%ld) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2.\n",
				    	i+1,(long)getpid(),chr,valuem1,valuem2);
				    sem_post(mshare);
				    //sem_post(full);
    			}
			}


    		
    		_exit(EXIT_SUCCESS);
    	}
    }

    //create vaccinators
    for(int i=0; i<v; i++){
    	vaccinator[i] = fork();
    	//check fork error
    	if(vaccinator[i] == -1){
    		perror("Error in vaccinator fork");
    		exit(1);
    	}

    	if(vaccinator[i] == 0){ // vaccinator code
    		int i = 0;
    		while(i<9){

	    		//sem_wait(full);
	    		sem_wait(m1);
	    		sem_wait(m2);
	    		sem_wait(mshare);
				printf("%ld remove 1 vaccine\n",(long) getpid());
				remove_1and2(addr);
			    sem_post(mshare);
			    sem_post(empty);
    			i++;
    		}

    		_exit(EXIT_SUCCESS);
    	}
    }

    //create citizen
    for(int i=0; i<c; i++){
    	citizen[i] = fork();
    	//check fork error
    	if(citizen[i] == -1){
    		perror("Error in citizen fork");
    		exit(1);
    	}

    	if(citizen[i] == 0){ // citizen code

    		//printf("citizen %ld\n", (long)getpid());
    		_exit(EXIT_SUCCESS);
    	}
    }
    
   	int total_process = n+v+c;
   	int status;
   	for(int i=0; i<total_process; i++){
   		wait(&status);
   	}
   	printf("parent end\n");
   	//close shared memory
   	printf("%s\n",addr );
   	munmap(addr,b);
   	//close semaphores
   	sem_close(mshare);
    sem_unlink("/mshare");

	sem_close(full);
    sem_unlink("/full");

    sem_close(empty);
    sem_unlink("/empty");

    sem_close(m1);
    sem_unlink("/m1");

	sem_close(m2);
    sem_unlink("/m2");


    shm_unlink("/vaccineBuf");
	return 0;
}


void insert_buffer(char* addr,char chr){

	for(int i=0; i<strlen(addr); i++){
		if(addr[i] == '-'){
			addr[i] = chr;
			break;
		}
	}
}

char remove_buffer(char* addr){
	for(int i=0; i<strlen(addr); i++){
		if(addr[i] == '1'){
			addr[i] = '-';
			return '1';
		}
		else if(addr[i] == '2'){
			addr[i] = '-';
			return '2';
		}
	}
	return 'N';
}

void remove_1and2(char* addr){
	int m1_check = 0;
	int m2_check = 0;
	for(int i=0; i<strlen(addr); i++){
		if(addr[i] == '1' && m1_check == 0){
			addr[i] = '-';
			m1_check = 1;
		}
		else if(addr[i] == '2' && m2_check == 0){
			addr[i] = '-';
			m2_check = 1;
		}

		if(m1_check == 1 && m2_check == 1) break;
	}
}