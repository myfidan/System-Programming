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
    printf("%d\n",n );
    printf("%d\n",v );
    printf("%d\n",c );
    printf("%d\n",b );
    printf("%d\n",t );
    printf("%s\n",inputfile );

    //first process
    if(fork() == 0){
    	while(1){
	    	if(flag > 0){
	    		printf("signal catcher\n");
	    		exit(0);
	    	}
    	}
    	
    	exit(0);
    }

    //second process
    if(fork() == 0){
    	while(1){
	    	if(flag > 0){
	    		printf("signal catcher\n");
	    		exit(0);
	    	}
    }
    	
    	exit(0);
    }
    // Parent process

    while(1){
    	if(flag > 0){
    		printf("signal catcher\n");
    		return 0;
    	}
    }

	return 0;
}