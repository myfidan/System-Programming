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
//macro for be ensure reading not interrupted by a signal
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);
#define BUF_SIZE 1024

void take_first_6_point(char* buffer,double* x_first_6,double* y_first_6,int numberCount);
double calculate_lagrande(double* x_first_6,double* y_first_6, int numberCount, int nextPoint);
void create_new_row(char* buffer,double res_6);
void parent_calculate_error(int fd,int x);
double take_x_7(char* buffer);

sig_atomic_t child_exit_status;
void clean_child_signal_handler(int signo){
	switch(signo){
		case SIGUSR1:

			break;
		case SIGCHLD:
			printf("hi\n");
			int status;
			wait(&status);
			child_exit_status = status;
			break;
	}
	
}

int main(int argc,char* argv[]){

	if(argc != 2){
		printf("Error argument\n");
		exit(1);
	}

	pid_t child_process[8];
	char* filepath = argv[1];
	
	int fd = open(filepath,O_RDWR | O_SYNC | O_APPEND);
	if(fd == -1){
		perror("open file error");
		exit(1);
	}

	struct sigaction signalAction;
	memset(&signalAction,0,sizeof(signalAction));
	signalAction.sa_handler = &clean_child_signal_handler;
	sigaction(SIGCHLD,&signalAction,NULL);
	sigaction(SIGUSR1,&signalAction,NULL);


	sigset_t sigset;
	sigfillset(&sigset);
	sigdelset(&sigset, SIGUSR1);
	
	sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);


	sigprocmask(SIG_BLOCK, &mask, NULL);

	//Create 8 process
	int i = 0;
	for(i=0; i<8; i++){
		child_process[i] = fork();
		if(child_process[i] == -1){
			perror("error in fork");
			exit(1);
			break;
		}
		else if(child_process[i] == 0){
			struct flock lock;
			memset(&lock,0,sizeof(lock));

			struct sigaction signalAction;
			memset(&signalAction,0,sizeof(signalAction));
			signalAction.sa_handler = &clean_child_signal_handler;
			sigaction(SIGUSR1,&signalAction,NULL);


			sigset_t sigset;
			sigfillset(&sigset);
			sigdelset(&sigset, SIGUSR1);
			
			sigset_t mask;
		    sigemptyset(&mask);
		    sigaddset(&mask, SIGUSR1);


			char buffer[BUF_SIZE];
			buffer[0] = '\0';

			int cnt;
			lock.l_type = F_WRLCK;
			//lock file 
			if(fcntl(fd,F_SETLKW,&lock) == -1){
				perror("fcntl error");
				exit(1);
			}
			lseek(fd,0,SEEK_SET);
			//NO_EINTR(cnt = read(fd,buffer,BUF_SIZE))
			

			char c[2];
			int count=1;
			char chr;
			//read(fd,buffer,BUF_SIZE);
			//Each prosess read corresponding row
			int k = 0;
			char beforeRow[BUF_SIZE];
			beforeRow[0] = '\0';
			char afterRow[BUF_SIZE];
			afterRow[0] = '\0';
			while((count = read(fd,&chr,sizeof(chr)))){
				if(k>i){
					strncat(afterRow,&chr,1);
					int count2 = 1;
					while((count2 = read(fd,&chr,sizeof(chr)))){
						strncat(afterRow,&chr,1);
					}
					break;
				}
				if (k == i){
					strncat(buffer,&chr,1);
				}
				else{
					strncat(beforeRow,&chr,1);
				}	
				if(chr == '\n') k++;
				//printf("%c",chr);
			}
			//printf("%s",buffer );
			char* useStrForToken = calloc(strlen(buffer)+1, sizeof(char));
			strcpy(useStrForToken, buffer);


			char* strNewRow = calloc(strlen(buffer)+100, sizeof(char));
			strcpy(strNewRow, buffer);


			
		   	
		   	//Arrays for first 6 coordinate
		   	double x_first_6[6];
		   	double y_first_6[6];
		   	double x_7;
		   	//Take 6 point
		   	take_first_6_point(useStrForToken,x_first_6,y_first_6,6);
		   	x_7 = take_x_7(buffer);
		   	double res_6 = calculate_lagrande(x_first_6,y_first_6,6,x_7);
		    create_new_row(strNewRow,res_6);

		    char * newWrite = calloc(1024, sizeof(char));
		    strcat(newWrite,beforeRow);
		    strcat(newWrite,strNewRow);
		    strcat(newWrite,afterRow);
		    //printf("%s\n",newWrite );
		    //printf("%s",beforeRow);
		    //printf("%s",strNewRow );
		    //printf("%s\n",afterRow);
		    //fflush(stdout);
		    /*
		    printf("Calculated points:\n");
		    for(int j=0; j<6; j++){
		    	printf("(%.1lf,%.1lf) ",x_first_6[j],y_first_6[j] );
		    }
		    printf("\n");
		    printf("Next Point %.1lf\n", res_6);
			*/

			lseek(fd,0,0);
			
			
			ftruncate(fd, 0);
			write(fd,newWrite,strlen(newWrite));
			
			lock.l_type = F_UNLCK;
			
			sigprocmask(SIG_BLOCK, &mask, NULL);
			//send sigusr1 to parent
			printf("Singal sending %d\n",i );
			kill(getppid(),SIGUSR1);
			
			
			if(fcntl(fd,F_SETLK,&lock) == -1){
				perror("fcntl error");
				exit(1);
			}
			
			//Wait signal from parent to continiue
			sigsuspend(&sigset);
			

			sigprocmask(SIG_UNBLOCK, &mask, NULL);

			/*  SECOND PART FOR CHILD PROCESSES */

			lock.l_type = F_WRLCK;
			//lock file 
			if(fcntl(fd,F_SETLKW,&lock) == -1){
				perror("fcntl error");
				exit(1);
			}

			lseek(fd,0,SEEK_SET);
			//NO_EINTR(cnt = read(fd,buffer,BUF_SIZE))
			
			printf("In process %d\n",i);
			count=1;
			//read(fd,buffer,BUF_SIZE);
			//Each prosess read corresponding row
			k = 0;
			char beforeRow_2[BUF_SIZE];
			char afterRow_2[BUF_SIZE];
			char buffer_2[BUF_SIZE];

			beforeRow_2[0] = '\0';
			afterRow_2[0] = '\0';
			buffer_2[0] = '\0';
			while((count = read(fd,&chr,sizeof(chr)))){
				if(k>i){
					strncat(afterRow_2,&chr,1);
					int count2 = 1;
					while((count2 = read(fd,&chr,sizeof(chr)))){
						strncat(afterRow_2,&chr,1);
					}
					break;
				}
				if (k == i){
					strncat(buffer_2,&chr,1);
				}
				else{
					strncat(beforeRow_2,&chr,1);
				}	
				if(chr == '\n') k++;
				//printf("%c",chr);
			}
			//printf("%s",buffer );

			printf("%s\n",buffer_2);
			double x_first_7[7];
		   	double y_first_7[7];
		   	char* useStrForToken_2 = calloc(strlen(buffer_2)+1,sizeof(char));
		   	strcpy(useStrForToken_2, buffer_2);

		   	char* strNewRow_2 = calloc(strlen(buffer_2)+100, sizeof(char));
			strcpy(strNewRow_2, buffer_2);

		   	take_first_6_point(useStrForToken_2,x_first_7,y_first_7,7);
		   	x_7 = take_x_7(buffer_2);
		   	double res_7 = calculate_lagrande(x_first_7,y_first_7,7,x_7);
		   	create_new_row(strNewRow_2,res_7);
		   	printf("%lf\n",res_7 );


		   	char * newWrite_2 = calloc(1024, sizeof(char));
		    strcat(newWrite_2,beforeRow_2);
		    strcat(newWrite_2,strNewRow_2);
		    strcat(newWrite_2,afterRow_2);


		    ftruncate(fd, 0);
			write(fd,newWrite_2,strlen(newWrite_2));
		    
		   	lock.l_type = F_UNLCK;
		
			if(fcntl(fd,F_SETLK,&lock) == -1){
				perror("fcntl error");
				exit(1);
			}

			free(useStrForToken_2);
			free(newWrite);
			free(strNewRow);
			free(useStrForToken);

			close(fd);

			sleep(5);
			//sigprocmask(SIG_UNBLOCK, &mask, NULL);
			_exit(EXIT_SUCCESS);
		}
		
	}

	//Waiting 8 signal from childrens with sigsuspend
	for(i = 0; i<8; i++){
		sigsuspend(&sigset);
		printf("arrive %d\n",i);
		
	}
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	parent_calculate_error(fd,7);
	
	//Sending sigusr1 to children 
	//So they can continiue
	for(int i=0; i<8; i++) kill(child_process[i],SIGUSR1);


	printf("Parent Finish\n");
	
	return 0;
}


//push first 6 x and first 6 y coordinat to xfirst_6 and yfirst_6 arrays
//Then I will use this 6 points arrays for calculating legrange polynomial
void take_first_6_point(char* buffer,double* x_first_6,double* y_first_6,int numberCount){
	//tokineze
	char s[2] = ",";
    char *token;
    /* get the first token */
    token = strtok(buffer, s);
   
    /* walk through other tokens */
    
    int x_index = 0;
    int y_index = 0;
    int j = 0;
    while( token != NULL ) {
    	double d;
    	sscanf(token, "%lf", &d);
    	if(j % 2 == 0){ //x point
    		if(x_index < numberCount){

	    		x_first_6[x_index] = d;
	    		x_index++;	
    		} 
    	}
    	else{ //y point
    		if(y_index < numberCount){

    			y_first_6[y_index] = d;
    			y_index++;	
    		}
    	}
    	j++;
    	token = strtok(NULL, s);
    }
}

//Calculate lagrande by using wikipedia formula for lagrande
double calculate_lagrande(double* x_first_6,double* y_first_6, int numberCount, int nextPoint){
	double res;
	double finalResult = 0;
	for(int i=0; i<numberCount; i++){
		res = 1;
		for(int j=0; j<numberCount; j++){
			if(i != j){
				res = res * (double)(nextPoint - x_first_6[j]) / (double)(x_first_6[i] - x_first_6[j]);
			}
		}
		finalResult += res * y_first_6[i];
	}

	return finalResult;
}

//Combine row with new finded lagrande value
void create_new_row(char* buffer,double res_6){
	//remove last \n
	buffer[strlen(buffer) - 1] = '\0';
	//Add comma
	char comma = ',';
	strncat(buffer,&comma,1);
	//add new value
	char d[15];
	sprintf(d,"%.1lf", res_6);
	strcat(buffer,d);
	//add \n again
	char newLine = '\n';
	strncat(buffer,&newLine,1);

}

double take_x_7(char* buffer){
	double x_7;
	char* temp = calloc(strlen(buffer)+1, sizeof(char));
	strcpy(temp, buffer);

	char s[2] = ",";
    char *token;
    /* get the first token */
    token = strtok(buffer, s);
   
    /* walk through other tokens */
    
    int j = 0;
    while( token != NULL ) {
    	if(j==14){
    		
    		sscanf(token,"%lf",&x_7);
    		break;
    	}
    	j++;
    	token = strtok(NULL, s);
    }

	free(temp);
	return x_7;
}

//Parent process calculate error and print terminal
void parent_calculate_error(int fd,int x){

	struct flock lock;
	memset(&lock,0,sizeof(lock));
	char buffer[BUF_SIZE];

	lock.l_type = F_RDLCK;
	//lock file 
	if(fcntl(fd,F_SETLKW,&lock) == -1){
		perror("fcntl error");
		exit(1);
	}
	lseek(fd,0,SEEK_SET);
	
	int count = 0;
	char chr;
	int comma_count = 0;
	char y_7[10];
	char p_7[10];
	double y7;
	double p7;
	double total_error = 0.0;
	while((count = read(fd,&chr,sizeof(chr)))){
		if( chr == '\n'){
			comma_count = 0;
			
			sscanf(y_7,"%lf",&y7);
			sscanf(p_7,"%lf",&p7);
			printf("%lf\n", fabs(y7-p7));
			total_error += fabs(y7-p7);
			y_7[0] = '\0';
			p_7[0] = '\0';				
		}
		else if(chr == ','){
			comma_count++;
		}
		else if(comma_count == 15){
			strncat(y_7,&chr,1);
		}
		else if(comma_count == 16){
			strncat(p_7,&chr,1);
		}

	}

	lseek(fd,0,0);
	lock.l_type = F_UNLCK;
	if(fcntl(fd,F_SETLK,&lock) == -1){
		perror("fcntl error");
		exit(1);
	}

	total_error = total_error/8.0;
	printf("Error of polynomial of degree 5: %.1lf\n",total_error );

}
