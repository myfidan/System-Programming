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


void create_deamon();
//This 2 using for create 1 instance of server
void create_one_instance();
void clear_sema();
void take_input(int argc,char* argv[]);
void err_exit(int errnum);
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
   // while(1){
	    struct sockaddr_in clientInf;
	    socklen_t clientSize = sizeof(clientInf);
	    cfd = accept(sfd,(struct sockaddr*)&clientInf,&clientSize);
	    if(cfd < 0){
	    	//print accept error
	    	err_exit(1);
	    }

	  
	    fprintf(logFile,"%d\n",port );
		fprintf(logFile,"%s\n",pathToLogFile );
		fprintf(logFile,"%d\n",poolSize);
		fprintf(logFile,"%s\n",datasetPath);
 //   }

	char buffer[1024];
	read(cfd,buffer,1024);
	// AFTER this is a deamon server..
	//sleep(5);
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
    dataFile = fopen(datasetPath,"r+");

    if(logFile == NULL || dataFile == NULL) err_exit(-1);
}

void err_exit(int errnum){
	clear_sema();
	exit(errnum);
}