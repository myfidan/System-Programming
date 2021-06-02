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

char IPv4[20];
int PORT;
char queryFilePath[255];
int clientID;

FILE* queryFile;
int querySize;

void take_input(int argc,char* argv[]);
void findQuerySize(char** queries);
void readQueries(char** queries);

int main(int argc, char *argv[]){
	
	take_input(argc,argv);


	struct sockaddr_in serverInf;
	int sfd;

	sfd = socket(AF_INET,SOCK_STREAM,0);
	if(sfd == -1){
		printf("Socket error\n");
		exit(1);
	}

	serverInf.sin_family = AF_INET;
	serverInf.sin_port = htons(PORT);
	
	if(inet_pton(AF_INET,IPv4,&serverInf.sin_addr) <= 0){
		printf("inet_pton error\n");
		exit(1);
	}

	if(connect(sfd,(struct sockaddr*)&serverInf,sizeof(serverInf)) < 0){
		printf("Connection failed\n");
		exit(1);
	}
	//Client now connect to server

  	char** queries;
  	findQuerySize(queries);
	queries = (char**)malloc(sizeof(char*)*querySize);
    readQueries(queries);

    for(int i=0; i<querySize; i++){
    	printf("%s\n",queries[i]);
    }
    
	sleep(6);
	printf("%s\n",IPv4 );
	printf("%d\n",PORT );
	printf("%s\n",queryFilePath );
	printf("%d\n",clientID );
	return 0;
}

void take_input(int argc,char* argv[]){
	if(argc != 9){
		printf("Wrong argument\n");
		exit(1);
	}
	int opt;
	extern char *optarg;
	extern int optind, optopt, opterr;
 	while((opt = getopt(argc, argv, "i:a:p:o:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'i': 
            	clientID = atoi(optarg);
                break;
            case 'a':
            	strcpy(IPv4,optarg); 
                break;
            case 'p': 
            	PORT = atoi(optarg);
                break;
            case 'o': 
            	strcpy(queryFilePath,optarg);
                break;

            default:
            	printf("Wrong argument\n");
            	exit(1);
        }  
    }
    //open log file
    queryFile = fopen(queryFilePath,"r");

    if(queryFile == NULL){
    	printf("No query File...\n" );
    	exit(1);
    } 
    	
}

void findQuerySize(char** queries){
	int totalQueryCount = 0;
	char* line = NULL;
	char *token;
	char stringID[5];
	sprintf(stringID,"%d",clientID);
	size_t len =0;
	ssize_t read;
	const char s[2] = " ";
	while ((read = getline(&line, &len, queryFile)) != -1) {
        token = strtok(line, s);
        if(strcmp(token,stringID) == 0) totalQueryCount++;
    }
    fclose(queryFile);
    querySize = totalQueryCount;
}

void readQueries(char** queries){
	char* line = NULL;
	char *token;
	char stringID[5];
	sprintf(stringID,"%d",clientID);
	size_t len =0;
	ssize_t read;
	const char s[2] = " ";

    queryFile = fopen(queryFilePath,"r");

    if(queryFile == NULL){
    	printf("No query File...\n" );
    	exit(1);
    }
    int index = 0;
    char tempBuff[1024];
    while ((read = getline(&line, &len, queryFile)) != -1) {
    	strcpy(tempBuff,line);
        token = strtok(line, s);
        if(strcmp(token,stringID) == 0){
        	queries[index] = (char*)malloc(sizeof(char)*1024);
        	strcpy(queries[index++],tempBuff);
        }
    }
}