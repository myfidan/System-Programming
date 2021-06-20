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
#include <time.h>

char IPv4[20];
int PORT;
char queryFilePath[255];
int clientID;

FILE* queryFile;
int querySize;

void take_input(int argc,char* argv[]);
void findQuerySize();
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
    time_t now;
    time(&now);
    char timeTemp[100];
    strcpy(timeTemp,ctime(&now));   
    timeTemp[strlen(timeTemp)-1] = '\0';
    printf("[%s]Client-%d connecting to %s\n",timeTemp,clientID,IPv4);
  	char** queries;
  	findQuerySize();
	queries = (char**)malloc(sizeof(char*)*querySize);
    readQueries(queries);

    char* buffer;
    int readC;
    for(int i=0; i<querySize; i++){
        time_t begin = time(NULL);
    	//printf("%s\n",queries[i]);
        time(&now);
        strcpy(timeTemp,ctime(&now));
        timeTemp[strlen(timeTemp)-1] = '\0';
    	printf("[%s]Client-%d connected and sending query '",timeTemp,clientID);
        for(int j=0; j<1024; j++){
            if(queries[i][j] != ';')
            printf("%c",queries[i][j]);
        }
        printf("'\n");
    	int checkWrite = write(sfd,queries[i],strlen(queries[i])+1);
        if(checkWrite == -1){
            printf("socket write error\n");
            exit(1);
        }
        //Read total byte
        int totalReadData = 0;
        char readsize[10];
        int totalsizeInt;
        readC = read(sfd,readsize,sizeof(readsize));
        if(readC == -1){
            printf("socket Read error\n");
            exit(1);
        }
        totalsizeInt = atoi(readsize);
        //read number of record
        int totalNumRec = 0;
        char numberRecord[10];
        readC = read(sfd,numberRecord,sizeof(numberRecord));
        if(readC == -1){
            printf("socket read error\n");
            exit(1);
        }
        totalNumRec = atoi(numberRecord);
        
        //Read column names
        char columnNames[1024];
        int k=0;
        while(k<1024){
            readC = read(sfd,columnNames,sizeof(columnNames));  
            if(readC == -1){
                printf("socket read error\n");
                exit(1);
            }
            k += readC;   
        }
        for(int j=0; j<1024; j++){
            if(columnNames[j] != '-')
            printf("%c",columnNames[j]);
        }
        //printf("%d\n",totalReadData );
        //printf("%s\n",readsize );
        buffer = (char*)malloc(sizeof(char)*totalsizeInt+1);
        buffer[0] = '\0';
        while(totalsizeInt != totalReadData){
            readC = read(sfd,buffer,sizeof(buffer));
            if(readC == -1){
                printf("socket read error\n");
                exit(1);
            }
            buffer[readC] = '\0';
            printf("%s",buffer );
            totalReadData += readC;
        }
        time_t end = time(NULL);
        time(&now);
        strcpy(timeTemp,ctime(&now));
        timeTemp[strlen(timeTemp)-1] = '\0';
        printf("[%s]Server's response to Client-%d is %d records, and arrived in %.2lf seconds\n",timeTemp,clientID,totalNumRec,(double)(end - begin));

        free(buffer);
    }
    time(&now);
    strcpy(timeTemp,ctime(&now));
    timeTemp[strlen(timeTemp)-1] = '\0';
    printf("[%s]A total of %d queries were executed, client is terminating.\n",timeTemp,querySize);
    if(close(sfd) == -1){
        printf("socket close error\n");
        exit(1);
    }

    if(fclose(queryFile) == -1){
        printf("fclose error\n");
        exit(1);
    }

    for(int i=0; i<querySize; i++){
        free(queries[i]);
    }
    free(queries);

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

void findQuerySize(){
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
    if(fclose(queryFile) == -1){
        printf("fclose error\n");
        exit(1);
    }
    querySize = totalQueryCount;
    free(line);
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
    char noktali = ';';
    while ((read = getline(&line, &len, queryFile)) != -1) {
    	strcpy(tempBuff,line);
        token = strtok(line, s);
        if(strcmp(token,stringID) == 0){
        	queries[index] = (char*)malloc(sizeof(char)*1024);
        	strcpy(queries[index],tempBuff);
            for(int i=strlen(queries[index]); i<1023; i++){
                strncat(queries[index],&noktali,1);
            }
            
            index++;
        }
    }
    free(line);
}