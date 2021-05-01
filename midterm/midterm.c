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

#define FIFO_PERM (S_IRUSR | S_IWUSR)

sig_atomic_t flag = 0;

void handler(int signal_number){
	++flag;
}


void insert_buffer(char* addr,char chr);
void remove_1and2(char* addr);
char remove_buffer(char* addr);
int number_of_vaccine_1(char* addr);
int number_of_vaccine_2(char* addr);
int remaining_citizen(char* helperAddr);
void decr_remaining_citizen(char* helperAddr);
void incr_vaccine_count(int vaccinator_num,char* helperAddr);
int take_number_of_vaccinator(char* third_shr);
void incr_number_of_vaccinator(char* third_shr);
void print_vaccinator_info(char* helperAddr,pid_t* vaccinator,int v);

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

    //Second shared memory for holding
    //information about how many vaccinator invite
    //How many citizen and remaining citizen count.
    int helper_shm = shm_open("/helper_shm", O_CREAT | O_RDWR,0666);
    if(helper_shm < 0){
    	perror("shm_open error");
    	return 1;
    }
    ftruncate(helper_shm,4096);//create b size buffer
    char* helperAddr = mmap(NULL,4096, PROT_READ | PROT_WRITE, MAP_SHARED, helper_shm,0); 
    helperAddr[0] = '\0';
    char temp[10];
    snprintf(temp,10,"%d-",c);
    strcat(helperAddr,temp);

    for(int i=1; i<=v; i++){
    	char temp[10];
    	snprintf(temp,10,"v%d_0-",i);
   		strcat(helperAddr,temp);
    }

    //third shared memory for counting vaccinators
    int third_shr = shm_open("/third_shr", O_CREAT | O_RDWR,0666);
    if(third_shr < 0){
    	perror("shm_open error");
    	return 1;
    }
    ftruncate(third_shr,10);//create b size buffer
    char* third_addr = mmap(NULL,10, PROT_READ | PROT_WRITE, MAP_SHARED, third_shr,0); 
    third_addr[0] = '\0';
    strcat(third_addr,"0");
    //binary semo for access shared memory
    sem_t* mshare;
    mshare = sem_open("/mshare",O_CREAT,0666,1);
   

    //Check buffer Empty or not
    sem_t* empty;
    empty = sem_open("/empty",O_CREAT,0666,b);
    //semaphore for '1' and '2'
    sem_t* m1;
    m1 = sem_open("/m1",O_CREAT,0666,0);
    sem_t* m2;
    m2 = sem_open("/m2",O_CREAT,0666,0);
    //Citizen semaphore
    sem_t* civ;
    civ = sem_open("/civ",O_CREAT,0666,0);
    //vaccinator semaphore
    sem_t* atomic_wait;
    atomic_wait = sem_open("/atomic_wait",O_CREAT,0666,0);
    // semaphore I use for syncronization between vaccinator and vaccines
    sem_t* removeVac;
    removeVac = sem_open("/removeVac",O_CREAT,0666,0);

    //fifos for transfer pid of citizen and vaccinator between each other
    if(mkfifo("fifo1",FIFO_PERM) == -1){
    	if(errno != EEXIST){
    		perror("fifo error");
    		return 1;
    	}
    }    
    //processes
    pid_t nurse[n];
    pid_t vaccinator[v];
    pid_t citizen[c];

    //open file
    int fd;
    while(((fd = open(inputfile,O_RDWR)) == -1) && (errno == EINTR));
    if(fd == -1){
    	perror("open file error");
    	return 1;
    }

    
	printf("Welcome to the CSE344 clinic. Number of citizens to vaccinate c=%d with t=%d doses.\n",c,t);

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
    		//lock for file
    		struct flock lock;
			memset(&lock,0,sizeof(lock));
    		while(1){
    			//Exclusive lock file to read 1 char
    			lock.l_type = F_WRLCK;
				if(fcntl(fd,F_SETLKW,&lock) == -1){
					perror("fcntl error");
					exit(1);
				}
				count = read(fd,&chr,sizeof(chr));
				//After reading 1 char open lock so other process can access file
    			lock.l_type = F_UNLCK;
				if(fcntl(fd,F_SETLK,&lock) == -1){
					perror("fcntl error");
					exit(1);
				}
				if(count == 0) break; //EOF 

    			if(chr != '\n'){
    				//accessing shared memory lock
    				sem_wait(empty);
    				sem_wait(mshare);
					insert_buffer(addr,chr);

					int valuem1,valuem2;
					valuem1 = number_of_vaccine_1(addr);
					valuem2 = number_of_vaccine_2(addr);	
				    printf("Nurse %d (pid=%ld) has brought vaccine %c: the clinic has %d vaccine1 and %d vaccine2.\n",
				    	i+1,(long)getpid(),chr,valuem1,valuem2);
				    int x;
				    sem_getvalue(removeVac,&x);
				    if(valuem1-x > 0 && valuem2-x > 0){
				    	sem_post(atomic_wait);
				    	sem_post(removeVac);
				    }
				    sem_post(mshare);
    			}
			}


    		//close semaphores
		   	sem_close(mshare);
		    sem_close(empty);
		    sem_close(m1);
			sem_close(m2);
		    sem_close(civ);
		    sem_close(removeVac);
		    sem_close(atomic_wait);

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
    		
    		while(1){

    			sem_wait(mshare);
    			int number_of_vac = take_number_of_vaccinator(third_addr);
    			if( number_of_vac != t*c) incr_number_of_vaccinator(third_addr);
    			sem_post(mshare);

    			if( number_of_vac == t*c){
    				//close semaphores
				   	sem_close(mshare);
				    sem_close(empty);
				    sem_close(m1);
					sem_close(m2);
				    sem_close(civ);
				    sem_close(removeVac);
				    sem_close(atomic_wait);

    				exit(0);
    			}

	    		sem_wait(atomic_wait);

			    

	    		//critical section
	    		//update shared memory
				sem_wait(mshare);

			
				sem_wait(removeVac);
				remove_1and2(addr);
				
				
				
				
				
				
		    	//read_civ_pid[readed_char_num] = '\0';
		    	
		    	sem_post(civ); // take a civ
		    	
		    	
		    	int fifoFd;
				while(((fifoFd = open("fifo1",O_RDONLY)) == -1) && (errno == EINTR));
		    	if(fifoFd == -1){
		    		perror("open fifo error");
		    		exit(1);
		    	}
		    	char read_civ_pid[10];
		    	read(fifoFd,read_civ_pid,sizeof(read_civ_pid));
		    	close(fifoFd);
		    	

		    	printf("Vaccinator %d (pid=%ld) is inviting citizen pid=%s\n",i+1,(long) getpid(),read_civ_pid);
		    	incr_vaccine_count(i+1,helperAddr);

		    	sem_post(m1);
		    	sem_wait(m2);
			    sem_post(mshare);


			    sem_post(empty);
				sem_post(empty);
    			
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
    		int cit_pid = (int) getpid();
    		char char_id[20];
    		for(int j=0; j<t; j++){
    			sem_wait(civ);

    			snprintf(char_id,10,"%d",cit_pid);
    			
    			int fifoFd;
    			while(((fifoFd = open("fifo1",O_WRONLY)) == -1) && (errno == EINTR));
		    	if(fifoFd == -1){
		    		perror("open fifo error");
		    		exit(1);
		    	}
		    	write(fifoFd,char_id,sizeof(char_id));
		    	close(fifoFd);
		    	

				sem_wait(m1);
    			printf("Citizen %d (pid=%ld) is vaccinated for the %dth time: the clinic has %d vaccine1 and %d vaccine2\n",
    				i,(long)getpid(),j+1,number_of_vaccine_1(addr),number_of_vaccine_2(addr));
    			if(j+1 == t){
    				int rem_cit = remaining_citizen(helperAddr);
    				printf("Citizen is leaving. Remaining citizens to vaccinate: %d\n", rem_cit-1);
    				decr_remaining_citizen(helperAddr);
    				if(rem_cit - 1 == 0){
    					printf("All Citizens have been vaccinated.\n");
    				}
    			}
    			sem_post(m2);
    			
    		}
    		//close semaphores
		   	sem_close(mshare);
		    sem_close(empty);
		    sem_close(m1);
			sem_close(m2);
		    sem_close(civ);
		    sem_close(removeVac);
		    sem_close(atomic_wait);

    		_exit(EXIT_SUCCESS);
    	}
    }
    int status;
    for(int i=0; i<n; i++){
    	waitpid(nurse[i],&status,0);
    }
    printf("Nurses have carried all vaccines to the buffer, terminating.\n");
   	int total_process = v+c;
   	
   	for(int i=0; i<total_process; i++){
   		wait(&status);
   	}
   	//close shared memory
   	print_vaccinator_info(helperAddr,vaccinator,v);
   	
   	munmap(addr,b);
   	munmap(helperAddr,4096);
   	munmap(third_addr,10);
   	//close semaphores
   	sem_close(mshare);
    sem_unlink("/mshare");

    sem_close(empty);
    sem_unlink("/empty");

    sem_close(m1);
    sem_unlink("/m1");

	sem_close(m2);
    sem_unlink("/m2");

    sem_close(civ);
    sem_unlink("/civ");

    sem_close(removeVac);
    sem_unlink("/removeVac");

    sem_close(atomic_wait);
    sem_unlink("/atomic_wait");

    shm_unlink("/vaccineBuf");
    shm_unlink("/helper_shm");
    shm_unlink("/third_shr");
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

int number_of_vaccine_1(char* addr){
	int count = 0;
	for(int i=0; i<strlen(addr); i++){
		if(addr[i] == '1') count++;
	}

	return count;
}

int number_of_vaccine_2(char* addr){
	int count = 0;
	for(int i=0; i<strlen(addr); i++){
		if(addr[i] == '2') count++;
	}

	return count;
}

int remaining_citizen(char* helperAddr){
	char temp[10];
	temp[0] = '\0';
	for(int i=0; i<strlen(helperAddr); i++){
		if(helperAddr[i] == '-') break;
		else{
			strncat(temp,&helperAddr[i],1);
		}
	}
	
	return atoi(temp);
}

void decr_remaining_citizen(char* helperAddr){
	int prev = remaining_citizen(helperAddr);
	prev--;
	char temp[4096];
	snprintf(temp,4096,"%d-",prev);
	int count = 0;
	for(int i=0; i<strlen(helperAddr); i++){
		if(helperAddr[i] == '-'){
			count++;
			break;
		}
		else{
			count++;
		}
	}

	for(int i=count; i<strlen(helperAddr); i++){
		strncat(temp,&helperAddr[i],1);
	}
	strcpy(helperAddr,temp);
}

void incr_vaccine_count(int vaccinator_num,char* helperAddr){
	char temp[4096];
	temp[0] = '\0';
	strcat(temp,helperAddr);
	helperAddr[0] = '\0'; //reset shared memory
	char del = '-';
	char* token;
	token = strtok(temp,"-");
	while(token != NULL){
		if(token[0] == 'v'){
			char take_vaccinator[6];
			char take_vac_num[6];
			take_vaccinator[0] = '\0';
			take_vac_num[0] = '\0';
			int flag = 0;
			for(int i=1; i<strlen(token); i++){
				if(token[i] == '_') flag = 1;
				if(flag == 0){

					strncat(take_vaccinator,&token[i],1);
				}
				else{
					if(token[i] != '_')
					strncat(take_vac_num,&token[i],1);
				}
			}
			int integer_vaccinator = atoi(take_vaccinator);
			int integer_take_vac_num = atoi(take_vac_num);
			if(vaccinator_num == integer_vaccinator){
				integer_take_vac_num++;
			}
			char newAdd[20];
			snprintf(newAdd,20,"v%d_%d-",integer_vaccinator,integer_take_vac_num);
			strcat(helperAddr,newAdd);
		}
		else{
			strcat(helperAddr,token);
			strncat(helperAddr,&del,1);
		}
		token = strtok(NULL,"-");
	}
}

int take_number_of_vaccinator(char* third_shr){
	return atoi(third_shr);
}

void incr_number_of_vaccinator(char* third_shr){
	int num = atoi(third_shr);
	num++;
	third_shr[0] = '\0';
	char temp[5];
	snprintf(temp,5,"%d",num);
	strcat(third_shr,temp);
}

   	
void print_vaccinator_info(char* helperAddr,pid_t* vaccinator,int v){
	int k = 0;
	char temp[4096];
	temp[0] = '\0';
	strcat(temp,helperAddr);

	char* token;
	token = strtok(temp,"-");
	
	while(token != NULL){
		if(token[0] == 'v'){
			char vac_num[10];
			vac_num[0] = '\0';
			char vac_dose_val[10];
			vac_dose_val[0] = '\0';
			int flag = 0;	
			for(int j=1; j<strlen(token); j++){
				if(token[j] == '_') flag = 1;
				if(flag == 0){
					strncat(vac_num,&token[j],1);
				}
				else{
					if(token[j] != '_'){
						strncat(vac_dose_val,&token[j],1);
					}
				}
			}
			printf("Vaccinator %d (pid=%ld) vaccinated %d doses.\n",k,(long)vaccinator[k],atoi(vac_dose_val));
			k++;
		}
		token = strtok(NULL,"-");
	}

}
