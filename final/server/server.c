#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>
#include <getopt.h>

#include <semaphore.h>
#include <sys/mman.h>
#include <pthread.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define BD_NO_CHDIR 01 /* Don't chdir("/") */
#define BD_NO_CLOSE_FILES 02 /* Don't close all open files */
#define BD_NO_REOPEN_STD_FDS 04 /* Don't reopen stdin, stdout, and
stderr to /dev/null */
#define BD_NO_UMASK0 010 /* Don't do a umask(0) */
#define BD_MAX_CLOSE 8192 /* Maximum file descriptors to close if
sysconf(_SC_OPEN_MAX) is indeterminate */

//queue 
struct node{
	int sockedID;
	struct node* next;
};

struct node* front = NULL;
struct node* back = NULL;

void enqueue(int id);
int dequeue();

struct sqlQuery{
	int select;
	int update;
	int distinct;
	char columns[1024];
	char whereCond[255];
};

void create_deamon();
//This 2 using for create 1 instance of server
void create_one_instance();
void clear_sema();
void take_input(int argc,char* argv[]);
void FillTable();
void freeTable();
void FillSqlStructure(struct sqlQuery* sqlValues,char* temp);
void err_exit(int errnum);

void* threadFun(void* arg);
void handle_client(int cfd);
void readDataBase(struct sqlQuery sqlValues,char* sqlResult);
void writeDataBase(struct sqlQuery sqlValues,char* sqlResult);


sig_atomic_t sig_flag = 0;
void handler(int signal_number){
	sig_flag++;
}

sem_t* init_semaphore;
//input parameters
int port;
char pathToLogFile[255];
int poolSize;
char datasetPath[255];
FILE* logFile;
FILE* dataFile;
//mutex for shared memory queue
pthread_mutex_t queMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condQue = PTHREAD_COND_INITIALIZER;
int queSize = 0;

//This is my data structure table
char*** table;
char* firstRow = NULL; //This is first Row(Column names)
char** fr; //Column names
int tableSize = 0;
int totalColm = 0;

int main(int argc, char *argv[]){
	//Check this is first instance or not
	create_one_instance();
	//make this server deamon
	create_deamon();
	
	//signal handler
	struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_handler = &handler;
	act.sa_flags = 0;

	if((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT,&act,NULL) == -1)){
		perror("Error signal handler for SIGINT");
		return -1;
	}

	take_input(argc,argv);
	FillTable();
	pthread_t threadPool[poolSize];
	//create Threads
	for(int i=0; i<poolSize; i++){
		pthread_create(&threadPool[i],NULL,threadFun,NULL);
	}
	//Create socket
	int sfd,cfd;
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd == -1){
		//print socket error
		err_exit(1);
	}
	//structure
	struct sockaddr_in serverInf;
	memset(&serverInf,0,sizeof(serverInf));
	serverInf.sin_family = AF_INET;
	serverInf.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverInf.sin_port = htons(port);
	//bind
	if(bind(sfd,(struct sockaddr*)&serverInf,sizeof(serverInf)) < 0){
		//print socket bind error
		err_exit(1);
	}
	//listen
	if(listen(sfd,10) < 0 ){
        //print listen error
        err_exit(1);
    }
    
    //accept
  	while(1){
	    struct sockaddr_in clientInf;
	    socklen_t clientSize = sizeof(clientInf);
	    cfd = accept(sfd,(struct sockaddr*)&clientInf,&clientSize);
	    if(sig_flag > 0) break;
	    if(cfd < 0){
	    	//print accept error
	    	err_exit(1);
	    }
	    //pthread_create(&thr[k++],NULL,threadFun,(void*)cfd);
	    pthread_mutex_lock(&queMutex);
	    queSize++;
	    enqueue(cfd);
	    pthread_cond_signal(&condQue);
	    pthread_mutex_unlock(&queMutex);
	    
 	}

	
	// AFTER this is a deamon server..
	//sleep(5);
	freeTable();
	clear_sema();

	return 0;
}

void create_deamon(){

	int maxfd,fd;
	switch(fork()){
		case -1: err_exit(-1);
		case 0: break;
		default: exit(0);
	}

	if(setsid() == -1)  err_exit(-1);

	switch(fork()){
		case -1:  err_exit(-1);
		case 0: break;
		default: exit(0);
	}

	if(!(0 & BD_NO_UMASK0))
		umask(0);

	if(!(0 & BD_NO_CLOSE_FILES)){
		maxfd = sysconf(_SC_OPEN_MAX);
		if(maxfd == -1){
			maxfd = BD_MAX_CLOSE;
		}
		for(fd = 0; fd < maxfd; fd++){
			close(fd);
		}
	}

	if(!(0 & BD_NO_REOPEN_STD_FDS)){
		close(STDIN_FILENO);
		fd = open("/dev/null",O_RDWR);
		if(fd != STDIN_FILENO)  exit(-1);
		if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)  err_exit(-1);
		if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)  err_exit(-1);
	}
}

void create_one_instance(){
	init_semaphore = sem_open("/init_one",O_CREAT | O_EXCL,0666,0);
	if(init_semaphore == SEM_FAILED){
		if(errno == EEXIST){
			printf("Can't run second server instance\n");
			exit(-1);
		}
		printf("semaphore error %s\n",strerror(errno));
		err_exit(-1);
	}
}

void clear_sema(){
	if(sem_close(init_semaphore) == -1){
		exit(1);
	}
	if(sem_unlink("/init_one") == -1){
		exit(1);
	}
}

void take_input(int argc,char* argv[]){
	if(argc != 9){
		//printf("wrong\n");
		err_exit(-1);
	}
	int opt;
	extern char *optarg;
	extern int optind, optopt, opterr;
 	while((opt = getopt(argc, argv, "p:o:l:d:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'p': 
            	port = atoi(optarg);
                break;
            case 'o': 
            	strcpy(pathToLogFile,optarg);
            	break; 
            case 'l': 
            	poolSize = atoi(optarg);
            	break;
            case 'd': 
            	strcpy(datasetPath,optarg);
                break;   
            default:
            	err_exit(-1);
        }  
    }
    //open log file
    logFile = fopen(pathToLogFile,"w");
    dataFile = fopen(datasetPath,"r");

    if(logFile == NULL || dataFile == NULL) err_exit(-1);
}

void err_exit(int errnum){
	clear_sema();
	exit(errnum);
}

void* threadFun(void* arg){

    
	while(1){
		pthread_mutex_lock(&queMutex);
		while(queSize <= 0)  
			pthread_cond_wait(&condQue, &queMutex);
		queSize--;
		int cfd = dequeue();
		pthread_mutex_unlock(&queMutex);
		
		handle_client(cfd);

	}
	
	return NULL;
}

void handle_client(int cfd){
	char buffer[1024];
	char sqlResult[1024*1024];
	
	while(1){
		int c = read(cfd,buffer,sizeof(buffer));
	    if(c == -1 || c == 0) break; //client end
	    //if(buffer == NULL) break;
	    
	    //fprintf(logFile,"%s\n",buffer );
	    //fflush (logFile);
	    sqlResult[0] = '\0';
	    char temp[1024];
	    strcpy(temp,buffer);
	    struct sqlQuery sqlValues;

	    FillSqlStructure(&sqlValues,temp); //parse sql quary into the structure
	    
	    if(sqlValues.select){ //Reader (SELECT)
	    	readDataBase(sqlValues,sqlResult);
  	
	    }
	    else{ // Writer (UPDATE)
	    	writeDataBase(sqlValues,sqlResult);
	    }
	    

  		fprintf(logFile, "%d\n%d\n%d\n%s\n%s\n",sqlValues.select,sqlValues.update,sqlValues.distinct,sqlValues.columns,sqlValues.whereCond);
	    fflush(logFile);
	    //fprintf(logFile, "%s\n",sqlResult );
	   	//char temp2[] = "sent from server";
	   	//strcpy(buffer,temp2);
	   	fprintf(logFile, "%d\n", strlen(sqlResult)+1);
	   	fflush(logFile);

	   	char sizeTemp[10];
	   	char rightDel = '-';
	   	sprintf(sizeTemp,"%d",strlen(sqlResult)+1);
	   	
	   	int endindex = 0;
	   	while(sizeTemp[endindex] != '\0'){
	   		endindex++;
	   	}
	   	fprintf(logFile, "%d\n", endindex);
	   		fflush(logFile);
	   	for(int i=endindex; i<9; i++){
	   		strncat(sizeTemp,&rightDel,1);
	   	}
	   	fprintf(logFile, "%s\n", sizeTemp);
	   		fflush(logFile);

	   	write(cfd,sizeTemp,strlen(sizeTemp)+1);
	   		
		if(write(cfd,sqlResult,strlen(sqlResult)+1) == -1){
			fprintf(logFile, "Write socket error\n" );
			fflush(logFile);
			err_exit(1);
		}
		bzero(buffer, 1024);
		//bzero(sqlResult,strlen(sqlResult));
	}
}

//Simple enqueue implementation for my que
void enqueue(int id){
	struct node* temp = (struct node*)malloc(sizeof(struct node));
	temp->sockedID = id;
	temp->next = NULL;
	if(back == NULL){
		front = temp;
	}
	else{
		back->next = temp;
	}
	back = temp;
}

//Simple dequeue implementation for my que
int dequeue(){
	if(front == NULL){
		return -2;
	}
	int x = front->sockedID;
	struct node* temp = front;
	front = front->next;
	if(front == NULL){
		back = NULL;
	}
	free(temp);
	return x;
}

void FillTable(){
	char * line = NULL;
    size_t len = 0;
    ssize_t read;
	const char s[2] = ",";
    char *token;
	
	read = getline(&firstRow, &len, dataFile);
	
	for(int i=0; firstRow[i] != '\0'; i++){
		if(firstRow[i] == ',') totalColm++;
	}
	totalColm++;

	fr = (char**)malloc(sizeof(char*)*totalColm);
	token=strtok(firstRow,s);
	int index = 0;
	while(token != NULL){
		if(token[strlen(token)-1] == '\n') token[strlen(token)-1] = '\0';
		fr[index] = (char*)malloc(sizeof(char)*255);
		strcpy(fr[index],token);
		token = strtok(NULL,s);
		index++;
	}


	int cap = 2;
	table = (char***)malloc(sizeof(char**)*cap);
	while((read = getline(&line,&len,dataFile))!=-1){
		if(tableSize == cap){
			table = (char***)realloc(table,sizeof(char**)*cap*2);
			cap *= 2;
		}
		table[tableSize] = (char**)malloc(sizeof(char*)*totalColm);
		int z=0;
		for(int k=0; k<totalColm; k++){
			table[tableSize][k] = (char*)malloc(sizeof(char)*255);
			table[tableSize][k][0] = '\0';
			int doubleq = 0;
			while(line[z] != '\0'){
				if(line[z] == ',' && doubleq == 0){
					z++;
					break;
				}
				if(line[z] == '"'){
					if(doubleq == 0){
						doubleq = 1;
					}
					else{
						doubleq = 0;
					}
				}
				strncat(table[tableSize][k],&line[z],1);
				z++;
			}
			
			//strcpy(table[tableSize][k],line);
			//table[tableSize][k] = line;
		}
		tableSize++;
	}
	/*
	for(int i=0; i<totalColm; i++){
    	fprintf(logFile,"%s\n",fr[i]);
    }
    
	for(int i = 0; i < tableSize; ++i){
        for(int j = 0; j < totalColm; ++j){
            fprintf(logFile,"%s\n", table[i][j]);
        }
    }
    fflush(logFile);
	*/
    fclose(dataFile);
	 if (line)
	    free(line);
}

void FillSqlStructure(struct sqlQuery* sqlValues,char* temp){
	const char s[2] = " ";
	char *token;
	token = strtok(temp,s);
	int sqlIndex = 0;

	sqlValues->select = 0;
	sqlValues->update = 0;
	sqlValues->distinct = 0;
	sqlValues->columns[0] = '\0';
	sqlValues->whereCond[0] = '\0';
	int updateWhere = 0;
	while(token != NULL){
		if(sqlIndex == 1){ //Check Select or Update
			char selectStr[] = "SELECT";
			if(strcmp(token,selectStr) == 0){ //SELECT
				sqlValues->select = 1;
			}
			else{ // UPDATE
				sqlValues->update = 1;
			}
		}
		if(sqlIndex == 2 && sqlValues->select == 1){ //Check DISTINC CONTROL
			char distinctStr[] = "DISTINCT";
			if(strcmp(token,distinctStr) == 0){
				sqlValues->distinct = 1;
			}
			else{
				strcat(sqlValues->columns,token);
			}
		}
		if(sqlIndex>2 && sqlValues->select == 1){
			char fromStr[] = "FROM";
			if(strcmp(token,fromStr) != 0){
				strcat(sqlValues->columns,token);
			}
			else{
				break;
			}
		}
		if(updateWhere == 1){
			strcat(sqlValues->whereCond,token);
		}
		if(sqlIndex>3 && sqlValues->update == 1 && updateWhere == 0){
			char whereStr[] = "WHERE";
			if(strcmp(token,whereStr) != 0){
				strcat(sqlValues->columns,token);
			}
			else{
				updateWhere = 1;
				
			}
		}
		

		token = strtok(NULL,s);
		sqlIndex++;
	}
}


void readDataBase(struct sqlQuery sqlValues,char* sqlResult){
	if(sqlValues.distinct == 0){ // NOT DISTINCT
		const char s[2] = ",";
  		char *token;

  		token = strtok(sqlValues.columns,s);
  		int* arr = (int*)malloc(sizeof(int)*totalColm);
  		int arrSize = 0;
	    if(strcmp(token,"*") != 0){
	    	while( token != NULL ) {
	    	//strcat(sqlResult,token);
		    	for(int i=0; i<totalColm; i++){
		    		if(strcmp(fr[i],token) == 0){
		    			arr[arrSize++] = i;
		    			break;
		    		}
		    	}
		    	token = strtok(NULL, s);
	    	}
	    }
	    else{ // SELECT *
	    	for(int i=0; i<totalColm; i++){
	    		arr[arrSize++] = i;
	    	}
	    }
	    
	    //Tabledan elemanları al
	    for(int i=0; i<tableSize; i++){
	    	for(int j=0; j<arrSize; j++){
	    		strcat(sqlResult,table[i][arr[j]]);
	    		strcat(sqlResult,"\t");
	    		//fprintf(logFile, "%s\t",table[i][arr[j]]);
	    	}
	    	//fprintf(logFile, "\n");
	    	strcat(sqlResult,"\n");
	    }
	}
	else{ // DISTINCT
		const char s[2] = ",";
  		char *token;

  		token = strtok(sqlValues.columns,s);
  		int* arr = (int*)malloc(sizeof(int)*totalColm);
  		int arrSize = 0;
	    if(strcmp(token,"*") != 0){

		    while( token != NULL ) {
		    	//strcat(sqlResult,token);
		    	for(int i=0; i<totalColm; i++){
		    		if(strcmp(fr[i],token) == 0){
		    			arr[arrSize++] = i;
		    			break;
		    		}
		    	}
		    	token = strtok(NULL, s);
		    }
	    }
	    else{
	    	for(int i=0; i<totalColm; i++){
	    		arr[arrSize++] = i;
	    	}
	    }
	    //Tabledan elemanları al
	    char rowTemp[1024];
	    
	    for(int i=0; i<tableSize; i++){
	    	rowTemp[0] = '\0';
	    	for(int j=0; j<arrSize; j++){
	    		strcat(rowTemp,table[i][arr[j]]);
	    		strcat(rowTemp,"\t");
	    		//fprintf(logFile, "%s\t",table[i][arr[j]]);
	    	}
	    	//fprintf(logFile, "\n");
	    	char* pch;
	    	pch = strstr(sqlResult,rowTemp);
	    	if(pch == NULL){
	    		strcat(sqlResult,rowTemp);
	    		strcat(sqlResult,"\n");
	    	}
	    	
	    }

	}
	//strcat(sqlResult,"select(read)");
}

void writeDataBase(struct sqlQuery sqlValues,char* sqlResult){
	
	char temp[1024];
	strcpy(temp,sqlValues.columns);
 	int* indexes;
 	char** values;
 	fprintf(logFile, "%s\n",temp );
 	values = (char**)malloc(sizeof(char*)*totalColm);
	
	int indSize = 0;
	int valueSize = 0;
	const char s[4] = ",='";
	char *token;
	
	token = strtok(temp, s);
	int k=0;
	while( token != NULL ) {
		if(k%2 == 0){ //left
			for(int i=0; i<totalColm; i++){
				if(strcmp(fr[i],token) == 0){
					if(indSize == 0){
						indexes = (int*)malloc(sizeof(int)*1);
						indexes[indSize] = i;
						indSize++;
					}
					else{
						indexes[indSize] = i;
						indexes = (int*)realloc(indexes,indSize++);
					}
				}
			}
		}
		else{ //right
			values[valueSize] = (char*)malloc(sizeof(char)*255);
			fprintf(logFile, "%s\n",token);
			strcpy(values[valueSize],token);
			valueSize++;
		}
		k++;
		token = strtok(NULL, s);
	}

		


	//Industry_code_NZSIOC='AA21'
	//parse input like this
	const char s4[2] = "=";
	

  	token = strtok(sqlValues.whereCond,s4);
  	char whereLeft[100];
  	strcpy(whereLeft,token);
	
    token = strtok(NULL, s4);
	char whereRight[100];
	strcpy(whereRight,token);

	const char s2[2] = "'";
	char* token2;    
	token2 = strtok(whereRight,s2);
	strcpy(whereRight,token2);

	fprintf(logFile, "LEFT =%s\n",whereLeft);
	fprintf(logFile, "RIGHT =%s\n",whereRight);
	int colmIndex = 0;
	for(int i=0; i<totalColm; i++){
		if(strcmp(fr[i],whereLeft) == 0){
			colmIndex = i;
			break;
		}
	}
	for(int i=0; i<tableSize; i++){
		
		if(strcmp(table[i][colmIndex],whereRight) == 0){ //Where condition check
			for(int j=0; j<valueSize; j++){
				strcpy(table[i][indexes[j]],values[j]);
			}
		}
		
	}
  	strcat(sqlResult,"update");
}




void freeTable(){
	for(int i=0;i<tableSize; i++){
    	for(int j=0; j<totalColm; j++){
	    	free(table[i][j]);
	    }
	    free(table[i]);
    }
    free(table);
    for(int i=0; i<totalColm; i++){
    	free(fr[i]);
    }
    free(fr);
}